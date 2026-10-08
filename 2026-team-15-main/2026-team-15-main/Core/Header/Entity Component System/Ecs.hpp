// ecs.hpp - the team's own small ECS (no third-party ECS library).
// Entities are ids; components live in per-type pools; systems use ForEach.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Core
{
	using Entity = std::uint32_t;
	enum : Entity
	{
		NULL_ENTITY = 0
	};

	class IPoolBase
	{
	public:
		virtual ~IPoolBase()               = default;
		virtual void Remove(Entity entity) = 0;
	};

	template <typename T>
	class Pool final : public IPoolBase
	{
	public:
		T& Add(Entity entity, T value)
		{
			const auto found = Index.find(entity);
			if (found != Index.end())
			{
				Data[found->second] = std::move(value);
				return Data[found->second];
			}
			Index[entity] = Data.size();
			Owners.push_back(entity);
			Data.push_back(std::move(value));
			return Data.back();
		}

		T* Find(Entity entity)
		{
			const auto found = Index.find(entity);
			return found == Index.end() ? nullptr : &Data[found->second];
		}

		const T* Find(Entity entity) const
		{
			const auto found = Index.find(entity);
			return found == Index.end() ? nullptr : &Data[found->second];
		}

		void Remove(Entity entity) override
		{
			const auto found = Index.find(entity);
			if (found == Index.end())
				return;
			const std::size_t slot = found->second;
			const std::size_t last = Data.size() - 1;
			if (slot != last)
			{
				Data[slot]          = std::move(Data[last]);
				Owners[slot]        = Owners[last];
				Index[Owners[slot]] = slot;
			}
			Data.pop_back();
			Owners.pop_back();
			Index.erase(entity);
		}

		const std::vector<Entity>& GetOwners() const { return Owners; }
		std::size_t                Size() const { return Data.size(); }

	private:
		std::vector<T>                          Data;
		std::vector<Entity>                     Owners;
		std::unordered_map<Entity, std::size_t> Index;
	};

	class Registry
	{
	public:
		Entity Create()
		{
			const Entity entity = NextId++;
			Alive.insert(entity);
			return entity;
		}

		bool IsAlive(Entity entity) const { return Alive.count(entity) != 0; }

		// Destroying an entity destroys all of its components.
		void Destroy(Entity entity)
		{
			if (Alive.erase(entity) == 0)
				return;
			for (auto& entry : Pools)
				entry.second->Remove(entity);
		}

		template <typename T>
		T& Add(Entity entity, T value)
		{
			return GetOrCreatePool<T>().Add(entity, std::move(value));
		}

		template <typename T>
		T* TryGet(Entity entity)
		{
			Pool<T>* pool = FindPool<T>();
			return pool ? pool->Find(entity) : nullptr;
		}

		template <typename T>
		const T* TryGet(Entity entity) const
		{
			const Pool<T>* pool = FindPool<T>();
			return pool ? pool->Find(entity) : nullptr;
		}

		template <typename T>
		bool Has(Entity entity) const
		{
			return TryGet<T>(entity) != nullptr;
		}

		template <typename T>
		void Remove(Entity entity)
		{
			if (Pool<T>* pool = FindPool<T>())
				pool->Remove(entity);
		}

		// Calls fn(entity, First&, Rest&...) for every entity that has all listed components.
		// Do not create or destroy entities inside fn.
		template <typename First, typename... Rest, typename Fn>
		void ForEach(Fn&& fn)
		{
			Pool<First>* first = FindPool<First>();
			if (first == nullptr)
				return;
			const std::vector<Entity> owners = first->GetOwners();
			for (const Entity entity : owners)
			{
				First* a = first->Find(entity);
				if (a == nullptr)
					continue;
				if (!(Has<Rest>(entity) && ...))
					continue;
				fn(entity, *a, *TryGet<Rest>(entity)...);
			}
		}

		template <typename First, typename... Rest, typename Fn>
		void ForEach(Fn&& fn) const
		{
			const Pool<First>* first = FindPool<First>();
			if (first == nullptr)
				return;
			for (const Entity entity : first->GetOwners())
			{
				const First* a = first->Find(entity);
				if (a == nullptr)
					continue;
				if (!(Has<Rest>(entity) && ...))
					continue;
				fn(entity, *a, *TryGet<Rest>(entity)...);
			}
		}

		std::size_t             EntityCount() const { return Alive.size(); }
		const std::set<Entity>& GetEntities() const { return Alive; }

	private:
		template <typename T>
		Pool<T>& GetOrCreatePool()
		{
			const std::type_index key(typeid(T));
			auto                  found = Pools.find(key);
			if (found == Pools.end())
				found = Pools.emplace(key, std::make_unique<Pool<T>>()).first;
			return *static_cast<Pool<T>*>(found->second.get());
		}

		template <typename T>
		Pool<T>* FindPool()
		{
			const auto found = Pools.find(std::type_index(typeid(T)));
			return found == Pools.end() ? nullptr : static_cast<Pool<T>*>(found->second.get());
		}

		template <typename T>
		const Pool<T>* FindPool() const
		{
			const auto found = Pools.find(std::type_index(typeid(T)));
			return found == Pools.end() ? nullptr : static_cast<const Pool<T>*>(found->second.get());
		}

		Entity                                                          NextId = 1;
		std::set<Entity>                                                Alive;
		std::unordered_map<std::type_index, std::unique_ptr<IPoolBase>> Pools;
	};
}
