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

#ifndef KOPI_JSON_DIR
#define KOPI_JSON_DIR "Assets/Json/"
#endif

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
    bool gStationShown = false; // placed, tracking and the card not covered: draw it, run game time
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

    // Shared by start-up (BuildWorld) and hot reload (ReloadDefinitions)
    constexpr char kManifestFile[] = KOPI_JSON_DIR "JsonManifest.json";

    Ecs::TableRules const& Tables()
    {
        static Ecs::TableRules tables;
        if (tables.empty()) RegisterAllTables(tables);
        return tables;
    }

    bool CheckDef(Ecs::Json const& def, Ecs::ValidationS const& ctx) { return world.scene.CheckDefinition(def, ctx); }

    bool LoadDefinitions(Ecs::ErrorChecker& errs)
    {
        return Ecs::Load_All(kManifestFile, world.db, errs, CheckDef, &Tables());
    }

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

        // The station is shown only when it can be trusted; while it is hidden, game time is held (M2G05).
        gStationShown = gTracking && ArIsStationShown();
        if (Core::isTrackingHeld == gStationShown)
        {
            Core::isTrackingHeld = !gStationShown;
            LOGI("Session time %s", Core::isTrackingHeld ? "held: station hidden (tracking lost, card covered or not placed)"
                                                         : "running again: station shown");
        }

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
        gStationShown = false;
#endif
    }

    bool BuildWorld()
    {

        if (world.built) return true;                                   // once only

        world.coordinator.Init();
        RegisterAllComponents(world.scene);
        world.systems = Ecs::RegisterAllSystems(world.coordinator, world.db);

        Ecs::ErrorChecker errs;
        auto check = [](Ecs::Json const& def, Ecs::ValidationS const& ctx)
        {
            return world.scene.CheckDefinition(def, ctx);
        };

        static Ecs::TableRules tables;          // rules for non-entity tables (TABLE_LIST)
        if (tables.empty()) RegisterAllTables(tables);

        if (!Ecs::Load_All(KOPI_JSON_DIR "JsonManifest.json", world.db, errs, check, &tables))
        {
            for (std::string const& e : errs) LOGE("ECS load error: %s", e.c_str());
            return false;
        }

        auto made = Ecs::LoadScene(world.coordinator, world.scene, world.db, KOPI_JSON_DIR "Scenes.json", errs);
        if (!made)
        {
            for (std::string const& e : errs) LOGE("ECS scene error: %s", e.c_str());
            return false;
        }
        LOGI("ECS: %zu entities spawned", made->size());
        world.built = true;

        world.runner.emplace(world.coordinator, world.scene, world.db, *world.systems.recipeCheck);
        if (!world.runner->StartFirst(errs))   // first scenario by "Order" in Scenarios.json
        {
            for (std::string const& e : errs) 
            {
                LOGE("ECS scenario error: %s", e.c_str());
            }

            return false;
        }

        return true;
    }

    void ReloadDefinitions(char const* str)
    {
        if (!world.built) {
            LOGE("Reload skipped (%s): definitions never loaded, see the load error above", str);
            return;
        }

        Ecs::ReloadResult r = Ecs::HotReload(world.scene, world.coordinator, world.db, kManifestFile, CheckDef, &Tables());
        if (!r.ok) {                                                           // world.db is left untouched
            LOGE("Reload rejected (%s), keeping previous data:", str);
            for (auto const& e : r.errs) LOGE("   %s", e.c_str());
            return;
        }

        for (auto const& e : r.errs) LOGE("Reload: %s", e.c_str());
        LOGI("Reloaded definitions (%s): %zu entities refreshed, %zu field(s) changed", str, r.refreshed, r.changes.size());

        // What the reload changed, grouped per entity, e.g.  entity 0 (DRINKS.Kopi):  Temp.Status: "Hot" -> "Warm"
        Ecs::Entity last = static_cast<Ecs::Entity>(-1);
        for (auto const& c : r.changes)
        {
            if (c.entity != last) { LOGI("  entity %u (%s):", c.entity, c.source.c_str()); last = c.entity; }

            if (c.before.is_null())     LOGI("    + %s = %s", c.field.c_str(), c.after.dump().c_str());
            else if (c.after.is_null()) LOGI("    - %s (was %s)", c.field.c_str(), c.before.dump().c_str());
            else                        LOGI("    %s: %s -> %s", c.field.c_str(), c.before.dump().c_str(), c.after.dump().c_str());
        }

        // A real ECS system reading the reloaded components (not the log above): proves the systems see the new data
        if (world.systems.display) world.systems.display->Show();
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

        // Running scenario, e.g. "Scenario: Normal (4 cups)"
        if (world.runner && !world.runner->Current().empty())
        {
            int const s = std::snprintf(buf + used, size - used, "Scenario: %s (%zu cups)\n",
                                        world.runner->Current().c_str(), world.runner->Orders().size());
            if (s > 0 && static_cast<UI32>(s) < size - used) used += static_cast<UI32>(s);
        }

        // One line per drink, so a hot reload (F5) is visible on screen, e.g. "  #0 Kopi (Hot)"
        for (Ecs::Entity e : world.coordinator.LivingEntities())
        {
            if (!world.coordinator.HasComponent<Name>(e) || !world.coordinator.HasComponent<Temp>(e)) continue;
            int const m = std::snprintf(buf + used, size - used, "  #%u %s (%s)\n", e,
                                        world.coordinator.GetComponent<Name>(e).Value.c_str(),
                                        world.coordinator.GetComponent<Temp>(e).Status.c_str());
            if (m < 0 || static_cast<UI32>(m) >= size - used) break;
            used += static_cast<UI32>(m);
        }

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
            MeshRef const& mesh = world.coordinator.GetComponent<MeshRef>(e);
            def.meshPath   = mesh.Path;
            def.shaderName = mesh.Shader; // how it is drawn comes from the data too (Render-System box, see objLoader.hpp)
            def.colour     = mesh.Colour;

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
        if (Core::mInput.GetSnapshot().Is.Keys.Pressed.test(SDL_SCANCODE_F5)) ReloadDefinitions("F5");

        // F6: next scenario in Scenarios.json order (includes scenarios added by a hot reload)
        // F7: ask DisplaySystem what it sees right now (use before and after an F5 to compare)
        if (Core::mInput.GetSnapshot().Is.Keys.Pressed.test(SDL_SCANCODE_F7) && world.built && world.systems.display)
            world.systems.display->Show();

        if (Core::mInput.GetSnapshot().Is.Keys.Pressed.test(SDL_SCANCODE_F6) && world.runner)
        {
            Ecs::ErrorChecker errs;
            if (world.runner->StartNext(errs)) LOGI("Scenario started: %s", world.runner->Current().c_str());
            else for (auto const& e : errs) LOGE("Scenario error: %s", e.c_str());
        }
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
    Timer::Init();
    Profiler::Init();

    LOGI("Working directory: %s", std::filesystem::current_path().string().c_str());
    return BuildWorld();
}

bool Core::Init()
{
    bool const ok = TryInitialize();
    if (!ok) LOG_ERROR("Core::Init failed to initialize");

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

    return ok;
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
    dt = isTrackingHeld ? 0.0f : timeScale * unscaledDt; // held = last frame's AR check (see ARTrackingState)
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
    static float loaderSeconds = 0.0f;
    loaderSeconds += unscaledDt; // real time: the loader keeps spinning while the game clock is paused
#if defined(__ANDROID__)
    // The station only when it can be trusted (placed, tracking, card not covered); otherwise the loader in the middle.
    bool const stationShown = gStationShown; // decided in ARTrackingState, which also holds game time
    unsigned int depthTexture = 0;
    float        depthUv[9];
    bool const   hasDepth = ArGetDepthTexture(&depthTexture, depthUv);
    Renderer::Get().SetDepth(hasDepth ? depthTexture : 0, hasDepth ? depthUv : nullptr); // hands hide the pieces
#else
    bool const stationShown = gStationShown;
#endif
    EquipmentScene::Get().SetStationShown(stationShown);
    if (gTracking)
    {
        EquipmentScene::Get().Draw(gView, gProjection);
    }
    if (!stationShown)
    {
        EquipmentScene::Get().DrawLoader(loaderSeconds, viewportW, viewportH);
    }
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
    UI32 n = std::snprintf(buf, size, "%s\nframe %u, dt = %.2f ms, fps = %.1f%s\n", placementStatus, frameCount, dt * 1000.0f,
                           frameRate, isTrackingHeld ? "  [TIME HELD]" : (isPaused ? "  [PAUSED]" : ""));
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