#include "render/kopiScene.hpp"

#include <cmath>

#include "render/glApi.hpp"
#include "sim/equipment/objLoader.hpp"

namespace Render
{
	namespace
	{
		constexpr char const* kEquipmentFile = "Assets/Json/Equipment.json";
		constexpr float       kPi            = 3.14159265f;

		Sim::Vec3 const kEye{0.0f, 0.35f, 0.7f};
		Sim::Vec3 const kTarget{0.0f, 0.05f, 0.0f};
		float const     kHalfWidth = 0.24f; // meters of table that must stay in view sideways
	}

	bool KopiScene::Init(Sim::TextReader const& readText, std::string& error)
	{
		std::lock_guard<std::mutex> guard(Lock);

		std::optional<std::string> const json = readText(kEquipmentFile);
		if (!json)
		{
			error = std::string("cannot read ") + kEquipmentFile;
			return false;
		}
		Sim::EquipmentLibrary library;
		if (!Sim::ParseEquipment(*json, kEquipmentFile, library, error) ||
		    !Sim::AttachMeshBounds(library, readText, error))
		{
			return false;
		}
		Interaction.SetEquipment(library);

		if (!Renderer.Init(error))
		{
			return false;
		}
		for (Sim::EquipmentDef const& def : library.Items)
		{
			std::optional<Sim::MeshData> const mesh = Sim::ParseObj(*readText(def.MeshPath));
			if (!mesh || !Renderer.AddMesh(def.Id, *mesh))
			{
				error = def.Id + ": mesh upload failed";
				return false;
			}
		}
		return true;
	}

	void KopiScene::Resize(int width, int height)
	{
		std::lock_guard<std::mutex> guard(Lock);
		Width  = width > 0 ? width : 1;
		Height = height > 0 ? height : 1;
		glViewport(0, 0, Width, Height);
	}

	Sim::Mat4 KopiScene::View() const { return Sim::LookAt(kEye, kTarget, {0.0f, 1.0f, 0.0f}); }

	Sim::Mat4 KopiScene::Projection() const
	{
		// 40 degrees vertical on a wide window; on a tall phone screen widen it so the whole kit still fits sideways.
		float const aspect   = static_cast<float>(Width) / static_cast<float>(Height);
		float const distance = Sim::Length(kEye - kTarget);
		float const halfTan  = std::fmax(std::tan(20.0f * kPi / 180.0f), kHalfWidth / (distance * aspect));
		float const fovDeg   = 2.0f * std::atan(halfTan) * 180.0f / kPi;
		return Sim::Perspective(fovDeg, aspect, 0.01f, 10.0f);
	}

	void KopiScene::Update(float dt)
	{
		std::lock_guard<std::mutex> guard(Lock);
		Interaction.Update(dt);
	}

	void KopiScene::Draw()
	{
		std::lock_guard<std::mutex> guard(Lock);
		Renderer.Draw(Interaction, View(), Projection());
	}

	void KopiScene::Frame(float dt)
	{
		Update(dt);
		Draw();
	}

	std::string KopiScene::Press(std::string const& action)
	{
		std::lock_guard<std::mutex> guard(Lock);
		Sim::InteractionInput input;
		input.Kind   = Sim::INPUT_KIND::BUTTON;
		input.Action = action;
		return Interaction.Handle(input, View(), Projection()).Message;
	}

	std::string KopiScene::Touch(int action, float x, float y)
	{
		if (action != 0)
		{
			return {};
		}
		std::lock_guard<std::mutex> guard(Lock);
		Sim::InteractionInput input;
		input.Kind      = Sim::INPUT_KIND::TOUCH;
		input.X         = x;
		input.Y         = y;
		input.ViewportW = Width;
		input.ViewportH = Height;
		return Interaction.Handle(input, View(), Projection()).Message;
	}
}
