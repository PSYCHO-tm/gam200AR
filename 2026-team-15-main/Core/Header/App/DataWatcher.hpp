// dataWatcher.hpp - polls a folder for changed files (hot reload on desktop).
#pragma once
#include <filesystem>
#include <string>

namespace Core
{
	class DataWatcher
	{
	public:
		explicit DataWatcher(std::string folder)
			: Folder(std::move(folder))
		{
			Poll();
		}

		// True when any file under the folder changed since the last call.
		bool Poll()
		{
			namespace Fs = std::filesystem;
			std::error_code    code;
			Fs::file_time_type newest{};
			std::size_t        count = 0;
			for (Fs::recursive_directory_iterator it(Folder, code), end; !code && it != end; it.increment(code))
			{
				if (!it->is_regular_file(code))
					continue;
				++count;
				const Fs::file_time_type stamp = it->last_write_time(code);
				if (stamp > newest)
					newest = stamp;
			}
			const bool changed = (newest != LastNewest) || (count != LastCount);
			LastNewest         = newest;
			LastCount          = count;
			return changed;
		}

	private:
		std::string                     Folder;
		std::filesystem::file_time_type LastNewest{};
		std::size_t                     LastCount = 0;
	};
}
