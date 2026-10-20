#include "test-json.hpp"
#include "sharedlib.hpp"

namespace Ecs{
/*
    [READING] Parsing via stringstream, returns a Json object if successful to save
*/
    std::optional<Json> DeserializeStream(std::istream& is){
    is >> std::ws;

    if(!is || is.peek() == std::istream::traits_type::eof()) {
        std::cerr << "[DeserializeStream] Given Stringstream " << " is empty\n";
        return std::nullopt;
    }
    
    try{
     return Json::parse(is);
    }

    catch(const Json::parse_error& e){
        std::cerr << "[DeserializeStream] Invalid Json Format: " << e.what() << std::endl;
        std::cerr << "Near Byte Position: " << e.byte << std::endl;
        return std::nullopt;
    }

    catch (const Json::exception& e) {
    std::cerr << "[DeserializeStream] Unexpected Json error: " << e.what() << '\n';
    return std::nullopt;
    }
}

/*
    [READING] Making a Json file (delegated) by passing a file name
*/
    std::optional<Json> DeserializePath(std::string const& filepath){
    std::ifstream ifs(filepath);
    if(!ifs.is_open()){
        std::cerr << " [DeserializePath] File " << filepath << " passed to read cannot be opened\n";
        return std::nullopt;
    }

    return DeserializeStream(ifs);
}


    /*
        [WRITING] Takes a Json object and transforms it into a readable Json string
    */
    bool SerializeObj(Json const& obj, std::string const& filepath, int indent){
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << " [SerializeObj] File " << filepath << " cannot be opened\n";
        return false;
    }
    file << obj.dump(indent);
    file.flush();
    return file.good();
}

/*
    [RETRIEVING] Passing values from a Json file to individual drink objects, accepts different params
// */
//     template <typename T>
//     std::optional<T> RetrieveValue(Json const& obj, std::string const& key){
   
//     if(!obj.is_object()) {
//         std::cerr << "[RetrieveValue] File " << obj << " does not exist\n";
//         return std::nullopt;
//     }

//     auto pos = obj.find(key);
//     if(pos == obj.end()){
//         std::cerr << "[RetrieveValue] Key " << key << " does not exist\n";
//         return std::nullopt;
//     }

//     try{
//         return pos->get<T>(); //nlh
//     }
//     catch(const Json::exception& e){
//         std::cerr << "[RetrieveValue] Key " << key << " failed to convert\n";
//         return std::nullopt;
//     }
// }
}