#include <string>

#include "sim/equipment/equipmentDef.hpp"
#include "sim/equipment/objLoader.hpp"
#include "testCheck.hpp"

using namespace Sim;

static bool Contains(std::string const& text, char const* part) { return text.find(part) != std::string::npos; }

int main()
{
	// The real data file loads, every mesh loads, and bounds look like real kopi gear (centimeters, not meters).
	EquipmentLibrary library;
	std::string      error;
	TEST_CHECK(LoadEquipmentFile(RepoRoot() + "/Assets/Json/Equipment.json", library, error));
	TEST_CHECK(library.Items.size() == 3);
	TEST_CHECK(AttachMeshBounds(library, RepoRoot(), error));
	EquipmentDef const* kettle = FindEquipment(library, "Kettle");
	TEST_CHECK(kettle != nullptr && kettle->HasBounds);
	if (kettle != nullptr)
	{
		float const height = kettle->LocalBounds.Max.y - kettle->LocalBounds.Min.y;
		TEST_CHECK(height > 0.05f && height < 0.5f);
		TEST_CHECK(kettle->Interactions.size() == 1 && kettle->Interactions[0].Action == "pour_water");
		TEST_CHECK(kettle->Interactions[0].Quantity == 220.0f);
	}
	TEST_CHECK(FindEquipment(library, "SockFilter") != nullptr &&
	           FindEquipment(library, "SockFilter")->Interactions.size() == 2);
	TEST_CHECK(FindEquipment(library, "Nope") == nullptr);

	// A missing mesh is reported with the equipment name and path.
	EquipmentLibrary broken;
	TEST_CHECK(
		ParseEquipment(R"({"EQUIPMENT":{"Ghost":{"Mesh":"Assets/Models/ghost.obj"}}})", "inline", broken, error));
	TEST_CHECK(!AttachMeshBounds(broken, RepoRoot(), error) && Contains(error, "Ghost") &&
	           Contains(error, "ghost.obj"));

	// Bad data gives a clear error and leaves the library untouched.
	EquipmentLibrary keep = library;
	TEST_CHECK(!ParseEquipment("{ nope", "x.json", keep, error) && Contains(error, "invalid JSON"));
	TEST_CHECK(!ParseEquipment(R"({"EQUIPMENT":{}})", "x.json", keep, error) && Contains(error, "EQUIPMENT"));
	TEST_CHECK(!ParseEquipment(R"({"EQUIPMENT":{"Kettle":{"Position":[0,0,0]}}})", "x.json", keep, error) &&
	           Contains(error, "Kettle.Mesh"));
	TEST_CHECK(
		!ParseEquipment(R"({"EQUIPMENT":{"Kettle":{"Mesh":"a.obj","Position":[0,0]}}})", "x.json", keep, error) &&
		Contains(error, "Kettle.Position"));
	TEST_CHECK(!ParseEquipment(R"({"EQUIPMENT":{"Kettle":{"Mesh":"a.obj","Scale":0}}})", "x.json", keep, error) &&
	           Contains(error, "Kettle.Scale"));
	TEST_CHECK(!ParseEquipment(R"({"EQUIPMENT":{"Kettle":{"Mesh":"a.obj","Interactions":{"pour":{"Quantity":-1}}}}})",
	                           "x.json", keep, error) &&
	           Contains(error, "Kettle.Interactions.pour.Quantity"));
	TEST_CHECK(keep.Items.size() == 3);
	TEST_CHECK(!LoadEquipmentFile(RepoRoot() + "/Assets/Json/NoSuchFile.json", keep, error) &&
	           Contains(error, "cannot open"));

	// OBJ parsing: quads become two triangles sharing 4 vertices, missing normals are generated, bad input fails.
	auto const quad = ParseObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
	TEST_CHECK(quad.has_value() && quad->Vertices.size() == 4 && quad->Indices.size() == 6);
	TEST_CHECK(quad.has_value() && quad->Vertices[0].Normal.z > 0.99f);
	TEST_CHECK(ParseObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n").has_value());
	TEST_CHECK(ParseObj("v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nf 1 2 3\r\n").has_value());
	TEST_CHECK(!ParseObj("v 0 0 0\nf 1 2 3\n").has_value());
	TEST_CHECK(!ParseObj("").has_value());
	TEST_CHECK(!LoadObj("does/not/exist.obj").has_value());
	return TestFinish("test_equipment");
}
