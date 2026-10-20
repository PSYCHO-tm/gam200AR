#pragma once

// M1G05 visuals: draws the equipment meshes where the InteractionEngine says they are, with a table plane under them.
// It only reads the engine (model matrices, selection, active errors) and never changes it.
// Needs a current GL context for every call except the constructor.

#include <string>
#include <unordered_map>

#include "sim/equipment/meshData.hpp"
#include "sim/interaction/interactionEngine.hpp"
#include "sim/math/mat4.hpp"

namespace Render
{
	class EquipmentRenderer
	{
	public:
		// Compiles the shader and forgets every mesh. Safe to call again after the GL context was lost and
		// recreated (Android does this on pause/resume): old handles are dropped, not deleted.
		bool Init(std::string& error);

		// Uploads one mesh under an equipment id. Call again with the same id to replace it.
		bool AddMesh(std::string const& id, Sim::MeshData const& mesh);

		// Clears color + depth, then draws the table and every equipment piece that has an uploaded mesh.
		// Selected equipment pulses yellow; equipment with an active error shows red.
		void Draw(Sim::InteractionEngine const& engine, Sim::Mat4 const& view, Sim::Mat4 const& projection) const;

	private:
		struct GpuMesh
		{
			unsigned int Vao        = 0;
			unsigned int Vbo        = 0;
			unsigned int Ibo        = 0;
			int          IndexCount = 0;
		};

		void DrawMesh(GpuMesh const& mesh, Sim::Mat4 const& model, Sim::Mat4 const& viewProjection, float const color[3],
		              float const tint[3], float tintAmount) const;

		unsigned int                           Program = 0;
		int                                    UMvp    = -1;
		int                                    UModel  = -1;
		int                                    UColor  = -1;
		int                                    UTint   = -1;
		int                                    UAmount = -1;
		std::unordered_map<std::string, GpuMesh> Meshes;
		GpuMesh                                Table;
	};
}
