#include "Entity Component System/Components.hpp"
#include "Entity Component System/Ecs.hpp"
#include "TestFramework.hpp"

using namespace Core;

KOPIT_TEST(EcsDestroyRemovesComponents)
{
	Registry     reg;
	Entity const e = reg.Create();
	reg.Add(e, Transform{});
	reg.Add(e, Name{"cup"});
	KOPIT_CHECK(reg.Has<Transform>(e));
	KOPIT_CHECK(reg.EntityCount() == 1);
	reg.Destroy(e);
	KOPIT_CHECK(!reg.Has<Transform>(e));
	KOPIT_CHECK(!reg.Has<Name>(e));
	KOPIT_CHECK(!reg.IsAlive(e));
	KOPIT_CHECK(reg.EntityCount() == 0);
}

KOPIT_TEST(EcsForEachNeedsAllComponents)
{
	Registry     reg;
	Entity const a = reg.Create();
	Entity const b = reg.Create();
	reg.Add(a, Transform{});
	reg.Add(a, Spinner{10.0f});
	reg.Add(b, Transform{});
	int visited = 0;
	reg.ForEach<Transform, Spinner>([&](Entity, Transform&, Spinner&) { ++visited; });
	KOPIT_CHECK(visited == 1);
}

KOPIT_TEST(EcsAddTwiceReplaces)
{
	Registry     reg;
	Entity const e = reg.Create();
	reg.Add(e, Spinner{1.0f});
	reg.Add(e, Spinner{2.0f});
	KOPIT_CHECK(reg.TryGet<Spinner>(e)->DegreesPerSecond == 2.0f);
}

KOPIT_TEST(EcsRemoveKeepsOtherEntities)
{
	Registry     reg;
	Entity const a = reg.Create();
	Entity const b = reg.Create();
	Entity const c = reg.Create();
	reg.Add(a, Spinner{1.0f});
	reg.Add(b, Spinner{2.0f});
	reg.Add(c, Spinner{3.0f});
	reg.Destroy(a);
	KOPIT_CHECK(reg.TryGet<Spinner>(b)->DegreesPerSecond == 2.0f);
	KOPIT_CHECK(reg.TryGet<Spinner>(c)->DegreesPerSecond == 3.0f);
}
