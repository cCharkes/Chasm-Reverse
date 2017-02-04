#pragma once

#include <glsl_program.hpp>
#include <polygon_buffer.hpp>
#include <texture.hpp>

#include "fwd.hpp"
#include "rendering_context.hpp"

namespace PanzerChasm
{

class TextDraw final
{
public:
	enum class FontColor
	{
		White= 0u,
		DarkYellow,
		Golden,
		YellowGreen,
	};

	enum class Alignment
	{
		Left,
		Center,
		Right,
	};

	static constexpr unsigned char c_slider_back_letter_code= 8u;
	static constexpr unsigned char c_slider_left_letter_code = 1u;
	static constexpr unsigned char c_slider_right_letter_code= 2u;
	static constexpr unsigned char c_slider_letter_code= 3u;

	TextDraw(
		const RenderingContext& rendering_context,
		const GameResources& game_resources );
	~TextDraw();

	unsigned int GetLineHeight() const;

	void Print(
		int x, int y,
		const char* text,
		unsigned int scale,
		FontColor color= FontColor::White,
		Alignment alignment= Alignment::Left );

private:
	struct Vertex
	{
		short xy[2];
		short tex_coord[2];
	};

private:
	const Size2 viewport_size_;
	unsigned char letters_width_[256];

	r_GLSLProgram shader_;
	r_Texture texture_;

	r_PolygonBuffer polygon_buffer_;
	std::vector<Vertex> vertex_buffer_;

};

} // namespace PanzerChasm
