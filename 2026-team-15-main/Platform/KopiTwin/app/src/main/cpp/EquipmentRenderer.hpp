#pragma once

// Minimal OpenGL ES 3.0 renderer for the Goal 5 equipment: lit meshes, the selected one tinted yellow and any
// one with an active error tinted red. Android-only on purpose (it needs GL ES), so it lives here and not in Core.
// Every function runs on the GL thread.

#include <cstdint>
#include <vector>

#include "interactionEngine.hpp"
#include "meshData.hpp"
#include "mat4.hpp"

namespace KopiInteraction
{
	class EquipmentRenderer
	{
	public:
		// The GL context was destroyed or recreated: forget every handle without deleting (the driver already did).
		void ContextLost();
		void Shutdown();

		// Uploads the meshes when 'generation' changed, then draws every piece of equipment.
		// Clears the depth buffer first and leaves depth testing off afterwards, so it can follow ARCore's drawing.
		void Draw(Sim::InteractionEngine const& engine, std::vector<Sim::MeshData> const& meshes,
		          std::uint32_t generation, Sim::Mat4 const& view, Sim::Mat4 const& projection);

	private:
		struct GpuMesh
		{
			unsigned int Vao        = 0;
			unsigned int Vbo        = 0;
			unsigned int Ebo        = 0;
			int          IndexCount = 0;
		};

		bool BuildProgram();
		void Upload(std::vector<Sim::MeshData> const& meshes);
		void FreeMeshes();

		unsigned int         Program            = 0;
		int                  MvpLocation        = -1;
		int                  ModelLocation      = -1;
		int                  ColorLocation      = -1;
		int                  TintLocation       = -1;
		int                  TintAmountLocation = -1;
		std::vector<GpuMesh> Gpu;
		std::uint32_t        UploadedGeneration = 0;
		bool                 HasUpload          = false;
	};
}
