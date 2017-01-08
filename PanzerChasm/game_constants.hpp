#pragma once

namespace PanzerChasm
{

// Experimental game constants here.
namespace GameConstants
{

// Screen sizeo fo original game.
constexpr unsigned int min_screen_width = 320u;
constexpr unsigned int min_screen_height= 200u;

constexpr unsigned int weapon_count= 8u;
constexpr unsigned int mine_weapon_number= 6u;

constexpr unsigned int backpack_item_id= 0u;
constexpr unsigned int mine_item_id= 30u;

// TODO - calibrate mines parameters.
constexpr float mines_activation_radius= 0.5f;
constexpr float mines_explosion_radius= 2.3f;
constexpr int mines_damage= 140;
constexpr float mines_preparation_time_s= 1.5f;

constexpr float player_eyes_level= 0.75f; // From ground
constexpr float player_height= 0.9f;
constexpr float player_radius= 60.0f / 256.0f;
constexpr float player_interact_radius= 100.0f / 256.0f;
constexpr float player_z_pull_distance= 1.0f / 4.0f;

// Ticks in second, when monsters recieve damage from death zones.
constexpr float death_ticks_per_second= 3.0f;

const float player_max_speed= 5.0f;

constexpr float walls_height= 2.0f;

constexpr float procedures_speed_scale= 1.0f / 10.0f;

const float animations_frames_per_second= 20.0f;
const float weapons_animations_frames_per_second= 30.0f;
const float sprites_animations_frames_per_second= 15.0f;

const float weapon_switch_time_s= 0.7f;

// TODO - calibrate rockets parameters.
const float rockets_gravity_scale= 1.5f;
const float rockets_speed= 10.0f;
const float fast_rockets_speed= 20.0f;

constexpr float vertical_acceleration= -9.8f;
constexpr float max_vertical_speed= 5.0f;

constexpr float particles_vertical_acceleration= -5.0f;

} // namespace GameConstants

} // namespace PanzerChasm
