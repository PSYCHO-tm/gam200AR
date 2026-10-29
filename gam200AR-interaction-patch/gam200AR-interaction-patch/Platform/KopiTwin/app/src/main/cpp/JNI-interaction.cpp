/**
 * JNI-interaction.cpp
 *
 * JNI bridge between the Kotlin screen (InteractionNative.kt / InteractionPanel.kt) and the interaction session in
 * Core/ (InteractionSession). This is the only place that knows about Android for this feature: Kotlin passes plain
 * strings and the C++ core answers with plain strings, so the core never sees an Android event type.
 *
 *   touch  : MainActivity -> NativeCore.nativeSetInput -> Core input -> EquipmentScene ray-cast -> session.Select
 *   button : InteractionPanel -> nativeInteractionButton -> session.Press -> timed event -> assessment core
 *   screen : InteractionPanel polls nativeInteraction* for the order, buttons, events and errors
 *
 * Kotlin object: com.example.kopitwin.InteractionNative
 * Threads: these run on the Android UI thread; the GL thread draws at the same time. InteractionSession has its own
 * lock, so nothing here needs one.
 */

#include <jni.h>

#include <cstdio>
#include <string>

#include "EquipmentScene.hpp"
#include "InteractionSession.hpp"
#include "orderView.hpp"

namespace
{
    InteractionSession& Session() { return EquipmentScene::Get().GetSession(); }

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

    std::string Number(double value, char const* format)
    {
        char text[32];
        std::snprintf(text, sizeof(text), format, value);
        return text;
    }
}

extern "C"
{
    // Id of the selected equipment, or "" when nothing is selected.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionSelected(JNIEnv* env,
                                                                                                    jobject /*thiz*/)
    {
        return ToJString(env, Session().Selected());
    }

    // One line per interaction of the selected equipment: "action|label|quantity|units". Kotlin builds its buttons
    // from it, so a new interaction in Equipment.json becomes a new button with no code change.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionButtons(JNIEnv* env,
                                                                                                   jobject /*thiz*/)
    {
        std::string lines;
        for (InteractionDef const& interaction : Session().Buttons())
        {
            lines += interaction.action + "|" + interaction.label + "|" + Number(interaction.quantity, "%g") + "|" +
                     interaction.units + "\n";
        }
        return ToJString(env, lines);
    }

    // A button press -> core-defined input -> timed event -> assessment core.
    // Returns "" when accepted, otherwise why it was rejected.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionButton(JNIEnv* env,
                                                                                                  jobject /*thiz*/,
                                                                                                  jstring action)
    {
        return ToJString(env, Session().Press(ToString(env, action)));
    }

    // Same, for a held pour: the quantity the player poured replaces the one in the data.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionButtonQuantity(
        JNIEnv* env, jobject /*thiz*/, jstring action, jfloat quantity)
    {
        return ToJString(env, Session().Press(ToString(env, action), static_cast<float>(quantity)));
    }

    // The current order and step as one line of text, produced by the C++ core.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionOrderText(JNIEnv* env,
                                                                                                     jobject /*thiz*/)
    {
        return ToJString(env, Sim::FormatOrderText(Session().Order()));
    }

    // What the assessment core said about the last interaction, right or wrong.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionMessage(JNIEnv* env,
                                                                                                   jobject /*thiz*/)
    {
        return ToJString(env, Session().LastMessage());
    }

    // The last timed events, newest last: "time | equipment | action object | quantity". Display only.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionEvents(JNIEnv* env,
                                                                                                  jobject /*thiz*/)
    {
        std::vector<TimedEvent> const history = Session().History();
        std::size_t const       first         = history.size() > 5 ? history.size() - 5 : 0;
        std::string             lines;
        for (std::size_t i = first; i < history.size(); ++i)
        {
            TimedEvent const& event = history[i];
            lines += Number(event.time, "%.1f") + "s | " + event.equipment + " | " + event.action + " " + event.object +
                     " | " + Number(event.quantity, "%g") + "\n";
        }
        return ToJString(env, lines);
    }

    // The mistakes being shown right now: "equipment|message". The renderer tints the same pieces red.
    JNIEXPORT jstring JNICALL Java_com_example_kopitwin_InteractionNative_nativeInteractionErrors(JNIEnv* env,
                                                                                                  jobject /*thiz*/)
    {
        std::string lines;
        for (ActiveError const& active : Session().Errors())
        {
            lines += active.equipment + "|" + active.message + "\n";
        }
        return ToJString(env, lines);
    }
}
