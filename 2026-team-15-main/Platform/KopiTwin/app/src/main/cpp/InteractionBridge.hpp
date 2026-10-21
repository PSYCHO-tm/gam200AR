#pragma once

// Shared state for the Goal 5 interaction engine on Android.
//
// JNI-interaction.cpp (UI thread: touches, buttons) and the GL thread (drawing) both use this, so every access
// holds Lock. This is the only place that connects the engine to the camera: a fixed test camera for now, and
// whatever ARCore's view and projection are once SetExternalCamera is called with them.

#include <cstdint>
#include <mutex>
#include <vector>

#include "interactionEngine.hpp"
#include "meshData.hpp"
#include "mat4.hpp"

namespace KopiInteraction
{
	struct SceneState
	{
		std::mutex                 Lock;
		Sim::InteractionEngine     Engine;
		std::vector<Sim::MeshData> Meshes;             // same order as Engine.GetEquipment().Items
		std::uint32_t              MeshGeneration = 0; // bumps whenever Meshes is replaced, so the renderer re-uploads
		int                        ViewportW      = 1;
		int                        ViewportH      = 1;
		bool                       UseExternalCamera = false;
		Sim::Mat4                  ExternalView;
		Sim::Mat4                  ExternalProjection;
	};

	SceneState& GetScene();

	// The camera used for BOTH drawing and picking, so a touch selects exactly what is on screen.
	// Fixed test camera looking at the table, unless an external (ARCore) camera was supplied.
	void GetCamera(SceneState const& scene, Sim::Mat4& view, Sim::Mat4& projection);
}
