/**
 * JNI-interaction.cpp
 *
 * JNI bridge between the Kotlin screen (InteractionNative.kt) and the Goal 5 interaction engine in Core/.
 * This is the only place that knows about Android: touches arrive as MotionEvent action codes and are converted to
 * Sim::InteractionInput here, so the core never sees an Android event type.
 *
 * Kotlin object: com.example.kopitwin.InteractionNative
 * Threads: touches and buttons arrive on the UI thread, drawing on the GL thread. Everything takes the scene lock.
 */

#include <jni.h>

#include <map>
#include <string>

#include "Engine.hpp"
#include "EquipmentRenderer.hpp"
#include "equipmentDef.hpp"
#include "objLoader.hpp"
#include "orderView.hpp"
#include "InteractionBridge.hpp"

namespace KopiInteraction
{
	SceneState& GetScene()
	{
		static SceneState scene;
		return scene;
	}

	void GetCamera(SceneState const& scene, Sim::Mat4& view, Sim::Mat4& projection)
	{
		if (scene.UseExternalCamera)
		{
			view       = scene.ExternalView;
			projection = scene.ExternalProjection;
			return;
		}
		float const aspect =
			static_cast<float>(scene.ViewportW) / static_cast<float>(scene.ViewportH > 0 ? scene.ViewportH : 1);
		view       = Sim::LookAt({0.0f, 0.35f, 0.7f}, {0.0f, 0.05f, 0.0f}, {0.0f, 1.0f, 0.0f});
		projection = Sim::Perspective(60.0f, aspect, 0.01f, 10.0f);
	}
}

namespace
{
	// android.view.MotionEvent action codes. Only this file knows them.
	constexpr jint gActionDown = 0;
	constexpr jint gActionUp   = 1;

	KopiInteraction::EquipmentRenderer gRenderer; // GL thread only

	std::string ToString(JNIEnv* env, jstring text)
	{
		if (text == nullptr)
		{
			return {};
		}
		char const* chars = env->GetStringUTFChars(text, nullptr);
		std::string result(chars != nullptr ? chars : "");
		if (chars != nullptr)
		{
			env->ReleaseStringUTFChars(text, chars);
		}
		return result;
	}

	jstring ToJString(JNIEnv* env, std::string const& text) { return env->NewStringUTF(text.c_str()); }

	bool CopyMatrix(JNIEnv* env, jfloatArray source, Sim::Mat4& destination)
	{
		if (source == nullptr || env->GetArrayLength(source) < 16)
		{
			return false;
		}
		env->GetFloatArrayRegion(source, 0, 16, destination.Values);
		return true;
	}

	std::string TimeText(double seconds)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "%.2f", seconds);
		return text;
	}
}

extern "C"
{
	// Loads Equipment.json plus the OBJ text of every mesh it names (Kotlin reads both from the APK's assets).
	// Returns "" on success, otherwise a message that names the equipment and the field. The old scene is kept on
	// failure.
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionLoad(
		JNIEnv* env, jobject /*thiz*/, jstring equipmentJson, jobjectArray meshPaths, jobjectArray meshTexts)
	{
		Sim::EquipmentLibrary library;
		std::string           error;
		if (!Sim::ParseEquipment(ToString(env, equipmentJson), "Equipment.json", library, error))
		{
			return ToJString(env, error);
		}

		std::map<std::string, std::string> objByPath;
		jsize const                        count = env->GetArrayLength(meshPaths);
		for (jsize i = 0; i < count && i < env->GetArrayLength(meshTexts); ++i)
		{
			jstring path                   = static_cast<jstring>(env->GetObjectArrayElement(meshPaths, i));
			jstring text                   = static_cast<jstring>(env->GetObjectArrayElement(meshTexts, i));
			objByPath[ToString(env, path)] = ToString(env, text);
			env->DeleteLocalRef(path);
			env->DeleteLocalRef(text);
		}

		std::vector<Sim::MeshData> meshes;
		for (Sim::EquipmentDef& item : library.Items)
		{
			auto found = objByPath.find(item.MeshPath);
			if (found == objByPath.end())
			{
				return ToJString(env, "Equipment '" + item.Id + "': mesh '" + item.MeshPath + "' was not provided");
			}
			std::optional<Sim::MeshData> mesh = Sim::ParseObj(found->second);
			if (!mesh)
			{
				return ToJString(env, "Equipment '" + item.Id + "': cannot read mesh '" + item.MeshPath + "'");
			}
			item.LocalBounds = Sim::ComputeBounds(*mesh);
			item.HasBounds   = true;
			meshes.push_back(std::move(*mesh));
		}

		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		scene.Engine.SetEquipment(std::move(library));
		scene.Meshes = std::move(meshes);
		++scene.MeshGeneration;
		return ToJString(env, "");
	}

	// GL thread, from onSurfaceCreated: the GL context is new, so every GL handle we held is gone.
	JNIEXPORT void JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionInitGl(JNIEnv* /*env*/,
	                                                                                           jobject /*thiz*/)
	{
		gRenderer.ContextLost();
	}

	JNIEXPORT void JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionResize(JNIEnv* /*env*/,
	                                                                                           jobject /*thiz*/,
	                                                                                           jint width, jint height)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		scene.ViewportW = width > 0 ? width : 1;
		scene.ViewportH = height > 0 ? height : 1;
	}

	// GL thread, once per frame after the camera picture: advances the engine by the core's frame time and draws.
	JNIEXPORT void JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionDraw(JNIEnv* /*env*/,
	                                                                                         jobject /*thiz*/)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		scene.Engine.Update(Core::dt);
		Sim::Mat4 view;
		Sim::Mat4 projection;
		KopiInteraction::GetCamera(scene, view, projection);
		gRenderer.Draw(scene.Engine, scene.Meshes, scene.MeshGeneration, view, projection);
	}

	// Optional: ARCore's view and projection (column-major, 16 floats each). Until this is called a fixed test camera
	// is used.
	JNIEXPORT void JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionSetCamera(
		JNIEnv* env, jobject /*thiz*/, jfloatArray view, jfloatArray projection)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		if (CopyMatrix(env, view, scene.ExternalView) && CopyMatrix(env, projection, scene.ExternalProjection))
		{
			scene.UseExternalCamera = true;
		}
	}

	// The anchor's pose (16 floats, column-major). Equipment is placed relative to it.
	JNIEXPORT void JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionSetAnchor(JNIEnv* env,
	                                                                                              jobject /*thiz*/,
	                                                                                              jfloatArray pose)
	{
		Sim::Mat4 anchor;
		if (!CopyMatrix(env, pose, anchor))
		{
			return;
		}
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		scene.Engine.SetAnchorPose(anchor);
	}

	// Touch -> core-defined input -> ray-cast pick. Returns the id of the selected equipment, or "".
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionTouch(JNIEnv* env,
	                                                                                             jobject /*thiz*/,
	                                                                                             jint action, jfloat x,
	                                                                                             jfloat y)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		if (action != gActionDown && action != gActionUp)
		{
			return ToJString(env, scene.Engine.GetSelected());
		}
		if (action == gActionDown)
		{
			Sim::InteractionInput input;
			input.Kind      = Sim::INPUT_KIND::TOUCH;
			input.X         = x;
			input.Y         = y;
			input.ViewportW = scene.ViewportW;
			input.ViewportH = scene.ViewportH;
			Sim::Mat4 view;
			Sim::Mat4 projection;
			KopiInteraction::GetCamera(scene, view, projection);
			scene.Engine.Handle(input, view, projection);
		}
		return ToJString(env, scene.Engine.GetSelected());
	}

	// Native button -> core-defined input -> the selected equipment's mapped interaction and a timed event.
	// Returns "" when accepted, otherwise why it was rejected.
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionButton(JNIEnv* env,
	                                                                                              jobject /*thiz*/,
	                                                                                              jstring action)
	{
		Sim::InteractionInput input;
		input.Kind                         = Sim::INPUT_KIND::BUTTON;
		input.Action                       = ToString(env, action);
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		Sim::Mat4                    view;
		Sim::Mat4                    projection;
		KopiInteraction::GetCamera(scene, view, projection);
		Sim::InteractionResult const result = scene.Engine.Handle(input, view, projection);
		return ToJString(env, result.Handled ? "" : result.Message);
	}

	// Id of the selected equipment, or "" when nothing is selected.
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionSelected(JNIEnv* env,
	                                                                                                jobject /*thiz*/)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		return ToJString(env, scene.Engine.GetSelected());
	}

	// One line per interaction of the selected equipment: "action|label|quantity|units". Kotlin builds its buttons from
	// it.
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionButtons(JNIEnv* env,
	                                                                                               jobject /*thiz*/)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		std::string                  lines;
		Sim::EquipmentDef const* selected = Sim::FindEquipment(scene.Engine.GetEquipment(), scene.Engine.GetSelected());
		if (selected != nullptr)
		{
			for (Sim::InteractionDef const& interaction : selected->Interactions)
			{
				lines += interaction.Action + "|" + interaction.Label + "|" +
				         std::to_string(static_cast<int>(interaction.Quantity)) + "|" + interaction.Units + "\n";
			}
		}
		return ToJString(env, lines);
	}

	// The current order and step as one line of text, produced by the C++ core.
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionOrderText(JNIEnv* env,
	                                                                                                 jobject /*thiz*/)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		return ToJString(env, Sim::FormatOrderText(scene.Engine.GetOrderView()));
	}

	// The last timed events, newest last: "time | object | action | quantity". Display only: nothing is consumed,
	// the assessment core still receives every event through DrainEvents().
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionEvents(JNIEnv* env,
	                                                                                              jobject /*thiz*/)
	{
		KopiInteraction::SceneState&        scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>         guard(scene.Lock);
		std::string                         lines;
		std::vector<Sim::TimedEvent> const& history = scene.Engine.GetHistory();
		std::size_t const                   first   = history.size() > 8 ? history.size() - 8 : 0;
		for (std::size_t i = first; i < history.size(); ++i)
		{
			lines += TimeText(history[i].Time) + " | " + history[i].Object + " | " + history[i].Action + " | " +
			         std::to_string(static_cast<int>(history[i].Quantity)) + "\n";
		}
		return ToJString(env, lines);
	}

	// The errors being shown right now: "object|message". Kotlin lists them; the renderer tints the object red.
	JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionErrors(JNIEnv* env,
	                                                                                              jobject /*thiz*/)
	{
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		std::string                  lines;
		for (Sim::ActiveError const& active : scene.Engine.GetActiveErrors())
		{
			lines += active.Error.Object + "|" + active.Error.Message + "\n";
		}
		return ToJString(env, lines);
	}

	// Normally the assessment core sets the order and reports errors. These two let the screen drive them by hand
	// while that core is not connected yet.
	JNIEXPORT void JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionSetOrder(
		JNIEnv* env, jobject /*thiz*/, jstring orderName, jstring stepName, jint stepIndex, jint stepCount,
		jfloat secondsLeft)
	{
		Sim::OrderView order;
		order.OrderName                    = ToString(env, orderName);
		order.StepName                     = ToString(env, stepName);
		order.StepIndex                    = stepIndex;
		order.StepCount                    = stepCount;
		order.StepSecondsLeft              = secondsLeft;
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		scene.Engine.SetOrderView(order);
	}

	JNIEXPORT void JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionShowError(
		JNIEnv* env, jobject /*thiz*/, jstring objectId, jstring kind, jstring message, jfloat seconds)
	{
		Sim::AssessmentError error;
		error.Object                       = ToString(env, objectId);
		error.Kind                         = ToString(env, kind);
		error.Message                      = ToString(env, message);
		KopiInteraction::SceneState& scene = KopiInteraction::GetScene();
		std::lock_guard<std::mutex>  guard(scene.Lock);
		scene.Engine.ReportError(error, seconds);
	}
}
