#pragma once
#include "sharedlib.hpp"
#include "Coordinator.hpp"
#include "test-json.hpp"

namespace Ecs{

    template<typename T>
    void LoadComponent(Coordinator& c, Entity e, Json const& j)
    {
        //Json function (.get) attaches itself to entity
        c.AddComponent<T>(e, j.get<T>());          
    }

    template<typename T>
    std::optional<Json> SaveComponent(Coordinator& c, Entity e)
    {
        if (!c.HasComponent<T>(e)) return std::nullopt;
        return Json(c.GetComponent<T>(e));         
    }

    // Holds the two functions for one component type
    struct IoParts{
        //(*load) is a function pointer that points to load
        void (*load)(Coordinator&, Entity, Json const&);

        //(*save) is a function pointer that points to save etc
        std::optional<Json> (*save)(Coordinator&, Entity);
        ComponentType type;
    };

    class JsonCoord{
        public: 
        explicit JsonCoord(Coordinator& c) noexcept: cord(c) {}
        
        template <typename T>
        void Register(std::string const& name);

        std::optional<Entity> Spawn(Json const& jsonfile, std::string const& path, ErrorChecker& errsvec);
        bool Saving(std::string const& path, ErrorChecker& errsvec);
        bool Loading(std::string const& path, ErrorChecker& errsvec);



        private:
        
        bool Apply(Entity e, Json const& jsonfile, std::string const& path, ErrorChecker& errsvec);
        bool LoadEntity(std::string const& idStr, Json const& j, std::string const& path, ErrorChecker& errsvec); //Loading 1 entity
        bool LoadingEntities(Json const& root, std::string const& path, ErrorChecker& errsvec); //Loading multiple entities for SS

        Coordinator& cord;
        std::unordered_map<std::string, IoParts> io{};
        std::vector<std::string> nameByType{};

        Json Snapshot();
    };
}

namespace Ecs{
    template <typename T>
    void JsonCoord::Register(std::string const& name){
        cord.RegisterComponent<T>();
        ComponentType t = cord.GetComponentType<T>();

        io[name] = IoParts{ &LoadComponent<T>, &SaveComponent<T>, t };

        if (nameByType.size() <= t) nameByType.resize(t + 1);
        nameByType[t] = name;
    }
}