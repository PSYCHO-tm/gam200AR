/* ======================================================================================
Main Author
Name:           Zachary Yee
Course:         IMGD
Digipen ID:     zachary.y
====================================================================================== */
#include <algorithm>
#include <memory>

#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include "GlApi.hpp"
#include "PlatformLog.hpp"
#include "test.hpp"
#include "ARcore.hpp"
#include "Engine.hpp"
#include "log.hpp"
#include "sharedlib.hpp"

#include "equipmentDef.hpp"
#if defined(KOPI_HAS_GL)
#include "EquipmentScene.hpp"
#include "Renderer.hpp"
#endif
#include "Time.hpp"
#include "Profiler.hpp"
#include "Coordinator.hpp"
#include "JsonSceneCoordinator.hpp"
#include "RegisterComponents.hpp"
#include "DefinitionLoader.hpp"
#include "RegisterSystem.hpp"
#include <filesystem>

static void PrintErrors(char const* title, Ecs::ErrorChecker const& errs) { // edited by Raphael
       LOG_ERROR(title);
       for (auto const& m : errs) LOG_ERROR("   " << m);
}


namespace
{
    constexpr int maxSteps{5};

    void UpdateAudio(InputSnapshot const& s)
    {
		auto& a = Core::audio;
		a.EnsureLoaded();

		// Music on/off
		if (s.Is.Keys.Pressed.test(SDL_SCANCODE_9) && !a.GetManifestMusic().empty())
		{
			auto const& track = a.GetManifestMusic().front();

			if (a.GetMusicName() == track && a.IsMusicPlaying()) a.StopMusic();
			else
				a.PlayMusic(track);
		}

		// Pause / resume music
		if (s.Is.Keys.Pressed.test(SDL_SCANCODE_P) && !a.GetMusicName().empty())
		{
			if (a.IsMusicPlaying()) a.PauseMusic();
			else
				a.ResumeMusic();
		}

		// Volume up / down
		if (s.Is.Keys.Pressed.test(SDL_SCANCODE_EQUALS)) a.SetMasterVolume(a.GetMasterVolume() + 0.1f);

		if (s.Is.Keys.Pressed.test(SDL_SCANCODE_MINUS)) a.SetMasterVolume(a.GetMasterVolume() - 0.1f);

		// Mute
		if (s.Is.Keys.Pressed.test(SDL_SCANCODE_M)) a.SetMuted(!a.IsMuted());

		// Play sound effects using keys 1-8
		auto const& sfx = a.GetManifestSfx();

		for (int i = 0; i < 8 && i < static_cast<int>(sfx.size()); ++i)
		{
			if (!s.Is.Keys.Pressed.test(SDL_SCANCODE_1 + i)) continue;

			a.PlaySfx(sfx[i]);
		}

		// Stop all sound effects using 0
		if (s.Is.Keys.Pressed.test(SDL_SCANCODE_0)) a.StopAllSfx();
	}

    // The camera of this frame. Update picks against it, so a tap hits what is on screen.
    bool gTracking = false;
    Matrix4x4 gView;
    Matrix4x4 gProjection;
    std::vector<Matrix4x4> planePoses;
    std::vector<Vector2D> planeSizes;

    struct World
    {
        Ecs::Coordinator coordinator;
        Ecs::JsonScene   scene{coordinator};
        Ecs::Database    db;
        Ecs::GameSystems systems;
        std::optional<Ecs::ScenarioRunner> runner;
        bool built = false;
    };
    World world;

    std::string gLastServe;
    // ARCore (and OpenGL) store 16 floats column-major for column vectors. That is exactly the memory layout of a
    // row-vector Matrix4x4, so the conversion is a plain copy.
    Matrix4x4 FromGL(float const* values)
    {
        Matrix4x4 matrix;
        std::memcpy(matrix.m, values, sizeof(matrix.m));
        return matrix;
    }
    void ARTrackingState()
    {
#if defined(__ANDROID__)
        ArCameraUpdate();

        float view[16];
        float projection[16];
        gTracking = ArGetCameraMatrices(view, projection);
        if (!gTracking)
        {
            EquipmentScene::Get().SetMarker(0, Matrix4x4{});
            EquipmentScene::Get().SetPlanes({}, {});
            return;
        }

        gView = FromGL(view);
        gProjection = FromGL(projection);

        // 1. Anchor Pose
        float anchorMatrix[16];
        if (ArGetAnchorPose(anchorMatrix))
        {
            EquipmentScene::Get().SetAnchorPose(FromGL(anchorMatrix));
        }

        // 2. Placement Marker
        float mMatrix[16] = {};
        int const markerState = ArGetPlacementMarker(mMatrix);
        EquipmentScene::Get().SetMarker(markerState, FromGL(mMatrix));

        // 3. Planes (Clear and reuse vectors to avoid heap allocations every frame!)
        planePoses.clear();
        planeSizes.clear();

        constexpr int maxPlanes = 8;
        float planeMatrices[maxPlanes * 16] = {};
        float planeExtents[maxPlanes * 2] = {};
        int const planeCount = ArGetPlanes(planeMatrices, planeExtents, maxPlanes);
        
        for (int i = 0; i < planeCount; ++i)
        {
            planePoses.push_back(FromGL(planeMatrices + i * 16));
            planeSizes.push_back(Vector2D{planeExtents[i * 2], planeExtents[i * 2 + 1]});
        }
        EquipmentScene::Get().SetPlanes(planePoses, planeSizes);
#else
        gTracking = false; // no ARCore on desktop
#endif
    }

    void BuildWorld()
    {

        if (world.built) return;                                   // once only

        world.coordinator.Init();
        RegisterAllComponents(world.scene);
        world.systems = Ecs::RegisterAllSystems(world.coordinator, world.db);

        Ecs::ErrorChecker errs;
        auto check = [](Ecs::Json const& def, std::string const& where, Ecs::ErrorChecker& e)
        {
            return world.scene.CheckDefinition(def, where, e);
        };
        if (!Ecs::Load_All("Assets/Json", world.db, errs, check))
        {
            for (std::string const& e : errs) LOGE("ECS load error: %s", e.c_str());
            return;
        }

        auto made = Ecs::LoadScene(world.coordinator, world.scene, world.db, "Assets/Json/Scenes.json", errs);
        if (!made)
        {
            for (std::string const& e : errs) LOGE("ECS scene error: %s", e.c_str());
            return;
        }
        LOGI("ECS: %zu entities spawned", made->size());
        world.built = true;

        world.runner.emplace(world.coordinator, world.scene, world.db, *world.systems.recipeCheck);
        if (!world.runner->Start("Tutorial", errs))
        {
            for (std::string const& e : errs) 
            {
                LOGE("ECS scenario error: %s", e.c_str());
            }
        }
    }

    UI32 FormatEcs(char* buf, UI32 size)
    {
        if (!world.built)
        {
            int const none = std::snprintf(buf, size, "ECS: not loaded\n");
            return (none < 0 || static_cast<UI32>(none) >= size) ? 0 : static_cast<UI32>(none);
        }

        int n = std::snprintf(buf, size, "ECS: %u entities\n", world.coordinator.NumLiving());
        if (n < 0 || static_cast<UI32>(n) >= size) return 0;
        UI32 used = static_cast<UI32>(n);

        return used;
    }

    EquipmentLibrary BuildEquipmentLibrary()
    {
        EquipmentLibrary library;
        for (Ecs::Entity e : world.coordinator.LivingEntities())
        {
            if (!world.coordinator.HasComponent<MeshRef>(e) || !world.coordinator.HasComponent<Transform3D>(e)) continue;

            EquipmentDef def;
            def.id       = world.coordinator.HasComponent<DefRef>(e) ? world.coordinator.GetComponent<DefRef>(e).Id : std::to_string(e);
            def.name     = world.coordinator.HasComponent<Name>(e) ? world.coordinator.GetComponent<Name>(e).Value : def.id;
            def.meshPath = world.coordinator.GetComponent<MeshRef>(e).Path;

            Transform3D const& t = world.coordinator.GetComponent<Transform3D>(e);
            def.position = t.position;
            def.yawDeg   = t.rotation.y;
            def.scale    = t.scale.x;

            if (world.coordinator.HasComponent<Props>(e))
            {
                auto const& values = world.coordinator.GetComponent<Props>(e).Values;
                auto        found  = values.find("Interactions");
                if (found != values.end() && found->second.is_object())
                {
                    for (auto const& [action, body] : found->second.items())
                    {
                        InteractionDef i;
                        i.action     = action;
                        i.label      = body.value("Label", action);
                        i.ingredient = body.value("Ingredient", std::string{});
                        i.quantity   = body.value("Quantity", 0.0f);
                        i.units      = body.value("Units", std::string{});

                        auto attrs = body.find("Attributes");
                        if (attrs != body.end() && attrs->is_object())
                        {
                            for (auto const& [k, v] : attrs->items())
                                if (v.is_string()) i.attributes[k] = v.get<std::string>();
                        }
                        
                        def.interactions.push_back(i);
                    }
                }
            }
            library.items.push_back(def);
        }
        std::sort(library.items.begin(), library.items.end(),
                  [](EquipmentDef const& a, EquipmentDef const& b) { return a.id < b.id; });
        return library;
    }

    EquipmentDef const* SelectedPiece()
    {
#if defined(KOPI_HAS_GL)
        auto const  selected = EquipmentScene::Get().GetSelected();
        auto const& items    = EquipmentScene::Get().GetLibrary().items;
        return (selected && *selected < items.size()) ? &items[*selected] : nullptr;
#else
        return nullptr; // no equipment scene without OpenGL
#endif
    }

    void FixedUpdate(F32 fixedDt) { /* physics, collision */ }
    void UpdateSystems(F32 const& dt)
    {
        Core::mInput.ProcessInput(dt);
        UpdateAudio(Core::mInput.GetSnapshot());
        ARTrackingState();
#if defined(KOPI_HAS_GL)
        EquipmentScene::Get().Update(dt, Core::mInput.GetSnapshot(), gTracking, gView, gProjection, Core::viewportW,
                                     Core::viewportH);
#endif
        /* camera, particles, animation, Ecs systems */
    }
}

Ecs::Coordinator& Core::GetWorld() 
{ return world.coordinator; }

bool Core::TryInitialize()
{
    bool ok = true;
    Timer::Init();
    Profiler::Init();

    std::cout << "Current working directory: " << std::filesystem::current_path().string();
    BuildWorld();
    return ok;
}

void Core::Init()
{
    TryInitialize();
	  audio.Init();
	  //audio.LoadManifest("Json/Audio.json");
	  audio.SetSfxVolume(0.8f);
#if defined(__ANDROID__)
    ArCameraInit();                  // camera texture + camera program (this is a new GL context)
#endif
#if defined(KOPI_HAS_GL)
    Renderer::Get().Init();          // forgets old GPU handles, then builds the lit shader
    EquipmentScene::Get().Init(BuildEquipmentLibrary());
#endif
}

void Core::Start()
{

}

void Core::Resize(int width, int height, int rotation)
{
#if defined(__ANDROID__)
    ArCameraResize(width, height, rotation);
#else
    (void)rotation;
#endif
#if defined(KOPI_HAS_GL)
    glViewport(0, 0, width, height);
#endif
    viewportW = width > 0 ? width : 1;
    viewportH = height > 0 ? height : 1;
}

void Core::Update(UI64 nowT)
{
    Profiler::BeginFrame();

    /** Update time variables */
    Timer::Tick(nowT);
    unscaledDt = std::min(static_cast<F32>(Timer::GetFrameTime()), maxDt);
    dt = timeScale * unscaledDt;
    frameCount = Timer::GetFrameCount();
    frameRate = static_cast<F32>(Timer::GetFrameRate());

    /** Check accumulator against fixedDt to ensure FixedUpdate happens 0-5 times depending on clamped dt. */
    accumulator += dt;
    // Per Frame.
    PROFILE_SCOPE("Update");
    UpdateSystems(dt);

    for (int steps = 0; steps < maxSteps && accumulator >= fixedDt; ++steps)
    {
        PROFILE_SCOPE("FixedUpdate");
        FixedUpdate(fixedDt);
        accumulator -= fixedDt;
    }
    // Hit maxSteps with time still owed: zero accum.
    if (accumulator >= fixedDt)
        accumulator = 0.f;

    Profiler::EndFrame();
}

void Core::Exit() {
    Core::GetWorld().DestroyAll();
    Profiler::LogReport();
    audio.Shutdown();
}
void Core::Render()
{
#if defined(__ANDROID__)
    ArCameraDraw();
#endif
#if defined(KOPI_HAS_GL)
    if (!gTracking) return;
    EquipmentScene::Get().Draw(gView, gProjection);
#endif
}

void Core::OnResume()
{
    Timer::Reset();
    //accumulator = 0.f;    // also forget any time owed from before the pause
}

UI32 Core::GetOverlayText(char* buf, UI32 size)
{
#if defined(__ANDROID__)
    char const* placementStatus = ArGetPlacementStatus();
#else
    char const* placementStatus = "AR unavailable on desktop";
#endif
    UI32 n = std::snprintf(buf, size, "%s\nframe %u, dt = %.2f ms, fps = %.1f\n", placementStatus, frameCount, dt * 1000.0f, frameRate);
    if (n < size)
    {
        n += FormatEcs(buf + n, size - n);                 // new
    }
    if (n < size)
    {
        n += Profiler::Format(buf + n, size - n);
    }
    return n;
}

UI32 Core::GetActionsText(char* buf, UI32 size)        // one "id<TAB>label" line per action
{
    EquipmentDef const* piece = SelectedPiece();
    UI32 used = 0;
    if (piece)
    {
        for (InteractionDef const& a : piece->interactions)
        {
            int const n = std::snprintf(buf + used, size - used, "%s\t%s\n", a.action.c_str(), a.label.c_str());
            if (n < 0 || static_cast<UI32>(n) >= size - used) break;
            used += static_cast<UI32>(n);
        }
    }
    if (used < size) buf[used] = '\0';
    return used;
}

void Core::DoAction(char const* actionId)
{
    EquipmentDef const* piece = SelectedPiece();
    if (!piece || !world.runner || world.runner->Orders().empty()) return;

    for (InteractionDef const& a : piece->interactions)
    {
        if (a.action != actionId) continue;
        Ecs::ErrorChecker errs;
        if (a.ingredient.empty())                      // no ingredient means "serve": score the order
        {
            Ecs::ScenarioReport report;
            if (world.runner->Finish(report, errs))
            {
                char text[64];
                std::snprintf(text, sizeof(text), "Served: %.0f points, grade %s", report.average, report.grade.c_str());
                gLastServe = text;
            }
        }
        else                                           // pour or add: put it in the cup
        {
            world.runner->Add(world.runner->Orders().front(), a.ingredient, a.quantity, a.attributes, errs);
        }
        for (std::string const& e : errs) LOGE("Interaction error: %s", e.c_str());
        return;
    }
}

void Core::PauseTime(bool p)
{
    isPaused = p;
    timeScale = isPaused ? 0.0f : 1.0f;
}