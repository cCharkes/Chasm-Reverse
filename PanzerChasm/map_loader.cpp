#include <cstring>

#include "assert.hpp"
#include "log.hpp"
#include "math_utils.hpp"

#include "map_loader.hpp"

namespace PanzerChasm
{

namespace
{

constexpr unsigned int g_bytes_per_floor_in_file= 86u * MapData::c_floor_texture_size;
constexpr unsigned int g_floors_file_header_size= 64u;

constexpr float g_map_coords_scale= 1.0f / 256.0f;

} // namespace

#pragma pack(push, 1)

struct MapWall
{
	unsigned char texture_id;
	unsigned char wall_size;
	unsigned char unknown;
	unsigned short vert_coord[2][2];
};

SIZE_ASSERT( MapWall, 11 );

struct MapLight
{
	unsigned short position[2u];
	unsigned short r0;
	unsigned short r1;
	unsigned char unknown[4u];
};

SIZE_ASSERT( MapLight, 12 );

struct MapMonster
{
	unsigned short position[2];
	unsigned short id;
	unsigned char angle; // bits 0-3 is angle. Other bits - unknown.
	unsigned char unknown;
};

SIZE_ASSERT( MapMonster, 8 );

#pragma pack(pop)

namespace
{

decltype(MapData::Link::type) LinkTypeFromString( const char* const str )
{
	if( std::strcmp( str, "link" ) == 0 )
		return MapData::Link::Link_;
	if( std::strcmp( str, "floor" ) == 0 )
		return MapData::Link::Floor;
	if( std::strcmp( str, "shoot" ) == 0 )
		return MapData::Link::Shoot;
	if( std::strcmp( str, "return" ) == 0 )
		return MapData::Link::Return;
	if( std::strcmp( str, "unlock" ) == 0 )
		return MapData::Link::Unlock;
	if( std::strcmp( str, "destroy" ) == 0 )
		return MapData::Link::Destroy;
	if( std::strcmp( str, "onofflink" ) == 0 )
		return MapData::Link::OnOffLink;

	return MapData::Link::None;
}

MapData::Procedure::ActionCommandId ActionCommandFormString( const char* const str )
{
	using Command= MapData::Procedure::ActionCommandId;

	if( std::strcmp( str, "lock" ) == 0 )
		return Command::Lock;
	if( std::strcmp( str, "unlock" ) == 0 )
		return Command::Unlock;
	if( std::strcmp( str, "playani" ) == 0 )
		return Command::PlayAnimation;
	if( std::strcmp( str, "stopani" ) == 0 )
		return Command::StopAnimation;
	if( std::strcmp( str, "move" ) == 0 )
		return Command::Move;
	if( std::strcmp( str, "xmove" ) == 0 )
		return Command::XMove;
	if( std::strcmp( str, "ymove" ) == 0 )
		return Command::YMove;
	if( std::strcmp( str, "rotate" ) == 0 )
		return Command::Rotate;
	if( std::strcmp( str, "up" ) == 0 )
		return Command::Up;
	if( std::strcmp( str, "light" ) == 0 )
		return Command::Light;
	if( std::strcmp( str, "waitout" ) == 0 )
		return Command::Waitout;


	return Command::Unknown;
}

} // namespace

MapLoader::MapLoader( const VfsPtr& vfs )
	: vfs_(vfs)
{}

MapLoader::~MapLoader()
{}

MapDataConstPtr MapLoader::LoadMap( const unsigned int map_number )
{
	if( map_number == last_loaded_map_number_ && last_loaded_map_ != nullptr )
		return last_loaded_map_;

	char map_file_name[16];
	char resource_file_name[16];
	char floors_file_name[16];
	char process_file_name[16];

	std::snprintf( map_file_name, sizeof(map_file_name), "MAP.%02u", map_number );
	std::snprintf( resource_file_name, sizeof(resource_file_name), "RESOURCE.%02u", map_number );
	std::snprintf( floors_file_name, sizeof(floors_file_name), "FLOORS.%02u", map_number );
	std::snprintf( process_file_name, sizeof(process_file_name), "PROCESS.%02u", map_number );

	const Vfs::FileContent map_file_content= vfs_->ReadFile( map_file_name );
	const Vfs::FileContent resource_file_content= vfs_->ReadFile( resource_file_name );
	const Vfs::FileContent floors_file_content= vfs_->ReadFile( floors_file_name );
	const Vfs::FileContent process_file_content= vfs_->ReadFile( process_file_name );

	if( map_file_content.empty() ||
		resource_file_content.empty() ||
		floors_file_content.empty() ||
		process_file_content.empty() )
	{
		return nullptr;
	}

	MapDataPtr result= std::make_shared<MapData>();

	LoadLevelScripts( process_file_content, *result );

	DynamicWallsMask dynamic_walls_mask;
	MarkDynamicWalls( *result, dynamic_walls_mask );

	// Scan map file
	LoadLightmap( map_file_content,*result );
	LoadWalls( map_file_content, *result, dynamic_walls_mask );
	LoadFloorsAndCeilings( map_file_content,*result );
	LoadMonsters( map_file_content, *result );

	// Scan resource file
	LoadModelsDescription( resource_file_content, *result );
	LoadWallsTexturesNames( resource_file_content, *result );

	// Scan floors file
	LoadFloorsTexturesData( floors_file_content, *result );

	// Cache result and return it
	last_loaded_map_number_= map_number;
	last_loaded_map_= result;
	return result;
}

void MapLoader::LoadLightmap( const Vfs::FileContent& map_file, MapData& map_data )
{
	const unsigned int c_lightmap_data_offset= 0x01u;

	const unsigned char* const in_data= map_file.data() + c_lightmap_data_offset;

	// TODO - tune formula
	for( unsigned int y= 0u; y < MapData::c_lightmap_size; y++ )
	for( unsigned int x= 0u; x < MapData::c_lightmap_size - 1u; x++ )
	{
		map_data.lightmap[ x + 1u + y * MapData::c_lightmap_size ]=
			255u - ( ( in_data[ x + y * MapData::c_lightmap_size ] - 3u ) * 6u );
	}
}

void MapLoader::LoadWalls( const Vfs::FileContent& map_file, MapData& map_data, const DynamicWallsMask& dynamic_walls_mask )
{
	const unsigned int c_walls_offset= 0x18001u;

	for( unsigned int y= 0u; y < MapData::c_map_size; y++ )
	for( unsigned int x= 1u; x < MapData::c_map_size; x++ )
	{
		const bool is_dynamic= dynamic_walls_mask[ x + y * MapData::c_map_size ];

		const MapWall& map_wall=
			*reinterpret_cast<const MapWall*>( map_file.data() + c_walls_offset + sizeof(MapWall) * ( x + y * MapData::c_map_size ) );

		if( map_wall.texture_id >= 128u )
		{
			const unsigned int c_first_item= 131u;
			const unsigned int c_first_model= 163u;

			if( map_wall.texture_id >= c_first_model )
			{
				map_data.static_models.emplace_back();
				MapData::Model& model= map_data.static_models.back();
				model.pos.x= float(map_wall.vert_coord[0][0]) * g_map_coords_scale;
				model.pos.y= float(map_wall.vert_coord[0][1]) * g_map_coords_scale;
				model.angle= float(map_wall.unknown & 7u) / 8.0f * Constants::two_pi + Constants::pi;
				model.model_id= map_wall.texture_id - c_first_model;
			}
			else if( map_wall.texture_id >= c_first_item )
			{
				map_data.items.emplace_back();
				MapData::Item& model= map_data.items.back();
				model.pos.x= float(map_wall.vert_coord[0][0]) * g_map_coords_scale;
				model.pos.y= float(map_wall.vert_coord[0][1]) * g_map_coords_scale;
				model.angle= float(map_wall.unknown & 7u) / 8.0f * Constants::two_pi + Constants::pi;
				model.item_id=  map_wall.texture_id - c_first_item;
			}
			continue;
		}

		if( !( map_wall.wall_size == 64u || map_wall.wall_size == 128u ) )
			continue;

		auto& walls_container= is_dynamic ? map_data.dynamic_walls : map_data.static_walls;
		walls_container.emplace_back();
		MapData::Wall& wall= walls_container.back();

		wall.vert_pos[0].x= float(map_wall.vert_coord[0][0]) * g_map_coords_scale;
		wall.vert_pos[0].y= float(map_wall.vert_coord[0][1]) * g_map_coords_scale;
		wall.vert_pos[1].x= float(map_wall.vert_coord[1][0]) * g_map_coords_scale;
		wall.vert_pos[1].y= float(map_wall.vert_coord[1][1]) * g_map_coords_scale;

		wall.vert_tex_coord[0]= 0.0f;
		wall.vert_tex_coord[1]= float(map_wall.wall_size) / 128.0f;

		wall.texture_id= map_wall.texture_id;
	} // for xy
}

void MapLoader::LoadFloorsAndCeilings( const Vfs::FileContent& map_file, MapData& map_data )
{
	const unsigned int c_offset= 0x23001u;

	for( unsigned int floor_or_ceiling= 0u; floor_or_ceiling < 2u; floor_or_ceiling++ )
	{
		const unsigned char* const in_data=
			map_file.data() + c_offset + MapData::c_map_size * MapData::c_map_size * floor_or_ceiling;

		unsigned char* const out_data=
			floor_or_ceiling == 0u
				? map_data.floor_textures
				: map_data.ceiling_textures;

		for( unsigned int x= 0u; x < MapData::c_map_size; x++ )
		for( unsigned int y= 0u; y < MapData::c_map_size; y++ )
		{
			const unsigned char texture_number= in_data[ y * MapData::c_map_size + x ];
			out_data[ x + y * MapData::c_map_size ]= texture_number;
		}
	}
}

void MapLoader::LoadMonsters( const Vfs::FileContent& map_file, MapData& map_data )
{
	const unsigned int c_lights_count_offset= 0x27001u;
	const unsigned int c_lights_offset= 0x27003u;
	unsigned short lights_count;
	std::memcpy( &lights_count, map_file.data() + c_lights_count_offset, sizeof(unsigned short) );

	const unsigned int monsters_count_offset= c_lights_offset + sizeof(MapLight) * lights_count;
	const unsigned int monsters_offset= monsters_count_offset + 2u;
	const MapMonster* map_monsters= reinterpret_cast<const MapMonster*>( map_file.data() + monsters_offset );
	unsigned short monster_count;
	std::memcpy( &monster_count, map_file.data() + monsters_count_offset, sizeof(unsigned short) );

	map_data.monsters.resize( monster_count );
	for( unsigned int m= 0u; m < monster_count; m++ )
	{
		MapData::Monster& monster= map_data.monsters[m];

		monster.pos.x= float(map_monsters[m].position[0]) * g_map_coords_scale;
		monster.pos.y= float(map_monsters[m].position[1]) * g_map_coords_scale;
		monster.monster_id= map_monsters[m].id - 100u;
		monster.angle=
			float( map_monsters[m].angle & 7u) / 8.0f * Constants::two_pi +
			1.5f * Constants::pi;
	}
}

void MapLoader::LoadModelsDescription( const Vfs::FileContent& resource_file, MapData& map_data )
{
	const char* start= std::strstr( reinterpret_cast<const char*>( resource_file.data() ), "#newobjects" );

	while( *start != '\n' ) start++;
	start++;

	const char* const end= std::strstr( start, "#end" );

	std::istringstream stream( std::string( start, end ) );

	while( !stream.eof() )
	{
		char line[ 512u ];
		stream.getline( line, sizeof(line), '\n' );

		if( stream.eof() )
			break;

		std::istringstream line_stream{ std::string( line ) };

		double num;
		line_stream >> num; // GoRad
		line_stream >> num; // Shad
		line_stream >> num; // BObj
		line_stream >> num; // BMPz
		line_stream >> num; // AC
		line_stream >> num; // Blw
		line_stream >> num; // BLmt
		line_stream >> num; // SFX
		line_stream >> num; // BSfx

		map_data.models_description.emplace_back();
		MapData::ModelDescription& model_description= map_data.models_description.back();

		line_stream >> model_description.file_name; // FileName

		model_description.animation_file_name[0u]= '\0';
		line_stream >> model_description.animation_file_name;
	}
}

void MapLoader::LoadWallsTexturesNames( const Vfs::FileContent& resource_file, MapData& map_data )
{
	for( char* const file_name : map_data.walls_textures )
		file_name[0]= '\0';

	const char* start= std::strstr( reinterpret_cast<const char*>( resource_file.data() ), "#GFX" );
	start+= std::strlen( "#GFX" );
	const char* const end= std::strstr( start, "#end" );

	std::istringstream stream( std::string( start, end ) );

	char line[ 512u ];
	stream.getline( line, sizeof(line), '\n' );

	while( !stream.eof() )
	{
		stream.getline( line, sizeof(line), '\n' );

		if( stream.eof() )
			break;

		std::istringstream line_stream{ std::string( line ) };

		unsigned int texture_number;
		line_stream >> texture_number;

		char colon[8];
		line_stream >> colon;

		line_stream >> map_data.walls_textures[ texture_number ];
	}
}

void MapLoader::LoadFloorsTexturesData( const Vfs::FileContent& floors_file, MapData& map_data )
{
	for( unsigned int t= 0u; t < MapData::c_floors_textures_count; t++ )
	{
		const unsigned char* const in_data=
			floors_file.data() + g_floors_file_header_size + g_bytes_per_floor_in_file * t;

		std::memcpy(
			map_data.floor_textures_data[t],
			in_data,
			MapData::c_floor_texture_size * MapData::c_floor_texture_size );
	}
}


void MapLoader::LoadLevelScripts( const Vfs::FileContent& process_file, MapData& map_data )
{
	const char* const start= reinterpret_cast<const char*>( process_file.data() );
	const char* const end= start + process_file.size();

	std::istringstream stream( std::string( start, end ) );

	char line[ 512 ];
	stream.getline( line, sizeof(line), '\n' );

	while( !stream.eof() )
	{
		stream.getline( line, sizeof(line), '\n' );
		if( stream.eof() || stream.fail() )
			break;

		std::istringstream line_stream{ std::string( line ) };

		char thing_type[ sizeof(line) ];
		line_stream >> thing_type;

		if( line_stream.fail() || thing_type[0] != '#' )
			continue;
		else if( std::strcmp( thing_type, "#mess" ) == 0 )
			LoadMessage( stream, map_data );
		else if( std::strcmp( thing_type, "#proc" ) == 0 )
			LoadProcedure( stream, map_data );
		else if( std::strcmp( thing_type, "#links" ) == 0 )
			LoadLinks( stream, map_data );
		else if( std::strcmp( thing_type, "#stopani" ) == 0 )
		{ /* TODO */ }

	} // for file

	return;
}

void MapLoader::LoadMessage( std::istringstream& stream, MapData& map_data )
{
	map_data.messages.emplace_back();
	MapData::Message& message= map_data.messages.back();

	while( !stream.eof() )
	{
		char line[ 512 ];
		stream.getline( line, sizeof(line), '\n' );

		if( stream.eof() )
			break;

		std::istringstream line_stream{ std::string( line ) };

		char thing[64];
		line_stream >> thing;
		if( std::strcmp( thing, "#end" ) == 0 )
			break;

		else if( std::strcmp( thing, "Delay" ) == 0 )
			line_stream >> message.delay_s;

		else if( std::strncmp( thing, "Text", std::strlen("Text") ) == 0 )
		{
			message.texts.emplace_back();
			MapData::Message::Text& text= message.texts.back();

			line_stream >> text.x;
			line_stream >> text.y;

			// Read line in ""
			char text_line[512];
			line_stream >> text_line;
			const int len= std::strlen(text_line);
			line_stream.getline( text_line + len, sizeof(text_line) - len, '\n' );

			text.data= std::string( text_line + 1u, text_line + std::strlen( text_line ) - 2u );
		}
	}
}

void MapLoader::LoadProcedure( std::istringstream& stream, MapData& map_data )
{
	map_data.procedures.emplace_back();
	MapData::Procedure& procedure= map_data.procedures.back();

	bool has_action= false;

	while( !stream.eof() )
	{
		char line[ 512 ];
		stream.getline( line, sizeof(line), '\n' );

		if( stream.eof() )
			break;

		std::istringstream line_stream{ std::string( line ) };

		char thing[64];
		line_stream >> thing;
		if( std::strcmp( thing, "#end" ) == 0 )
			break;

		else if( std::strcmp( thing, "#StartDelay" ) == 0 )
			line_stream >> procedure.start_delay_s;
		else if( std::strcmp( thing, "#BackWait" ) == 0 )
			line_stream >> procedure.back_wait_s;
		else if( std::strcmp( thing, "#Speed" ) == 0 )
			line_stream >> procedure.speed;
		else if( std::strcmp( thing, "#LifeCheckon" ) == 0 )
			line_stream >> procedure.life_check;
		else if( std::strcmp( thing, "#Mortal" ) == 0 )
			line_stream >> procedure.mortal;
		else if( std::strcmp( thing, "#LightRemap" ) == 0 )
			line_stream >> procedure.light_remap;
		else if( std::strcmp( thing, "#Lock" ) == 0 )
			line_stream >> procedure.locked;
		else if( std::strcmp( thing, "#Loops" ) == 0 )
			line_stream >> procedure.loops;
		else if( std::strcmp( thing, "#LoopDelay" ) == 0 )
			line_stream >> procedure.loop_delay_s;
		else if( std::strcmp( thing, "#OnMessage" ) == 0 )
			line_stream >> procedure.on_message_number;
		else if( std::strcmp( thing, "#FirstMessage" ) == 0 )
			line_stream >> procedure.first_message_number;
		else if( std::strcmp( thing, "#LockMessage" ) == 0 )
			line_stream >> procedure.lock_message_number;
		else if( std::strcmp( thing, "#SfxId" ) == 0 )
			line_stream >> procedure.sfx_id;
		else if( std::strcmp( thing, "#SfxPosxy" ) == 0 )
		{
			line_stream >> procedure.sfx_pos[0];
			line_stream >> procedure.sfx_pos[1];
		}
		else if( std::strcmp( thing, "#LinkSwitchAt" ) == 0 )
		{
			line_stream >> procedure.link_switch_pos[1];
		}
		else if( std::strcmp( thing, "#action" ) == 0 )
			has_action= true;
		else if( has_action )
		{
			const auto commnd_id= ActionCommandFormString( thing );
			if( commnd_id == MapData::Procedure::ActionCommandId::Unknown )
				Log::Warning( "Unknown coommand: ", thing );
			else
			{
				procedure.action_commands.emplace_back();
				auto& command= procedure.action_commands.back();

				command.id= commnd_id;

				unsigned int arg= 0u;
				while( !line_stream.fail() )
				{
					line_stream >> command.args[arg];
					arg++;
				}
			}
		} // if has_action

	} // for procedure
}

void MapLoader::LoadLinks( std::istringstream& stream, MapData& map_data )
{
	while( !stream.eof() )
	{
		char line[ 512 ];
		stream.getline( line, sizeof(line), '\n' );

		if( stream.eof() )
			break;

		std::istringstream line_stream{ std::string( line ) };

		char link_type[32];
		line_stream >> link_type;
		if( std::strcmp( link_type, "#end" ) == 0 )
			break;

		unsigned short x, y;
		line_stream >> x;
		line_stream >> y;

		MapData::Link& link= map_data.links[ x + y * MapData::c_map_size ];

		line_stream >> link.proc_id;
		link.type= LinkTypeFromString( link_type );
	}
}

void MapLoader::MarkDynamicWalls( const MapData& map_data, DynamicWallsMask& out_dynamic_walls )
{
	for( bool& wall_is_dynamic : out_dynamic_walls )
		wall_is_dynamic= false;

	for( const MapData::Procedure& procedure : map_data.procedures )
	{
		for( const MapData::Procedure::ActionCommand& command : procedure.action_commands )
		{
			using Command= MapData::Procedure::ActionCommandId;
			if( command.id == Command::Move ||
				command.id == Command::Rotate ||
				command.id == Command::Up )
			{
				const unsigned int y= static_cast<unsigned int>(command.args[0]);
				const unsigned int x= static_cast<unsigned int>(command.args[1]);
				if( x < MapData::c_map_size &&
					y < MapData::c_map_size )
				{
					out_dynamic_walls[ x + y * MapData::c_map_size ]= true;
				}
			}
		} // for commands
	} // for procedures
}

} // namespace PanzerChasm
