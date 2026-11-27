/* Unpacking the compiler pack and the source zip (miniz, MIT licence). */
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace unzip {

/* Unpacks every file of ZIP into DEST (made if missing). Program flags kept on
 * Linux. progress(done_bytes, total_bytes) is called now and then; return false
 * from it to stop. Names that would land outside DEST are refused. */
bool extract_all(const std::filesystem::path& zip, const std::filesystem::path& dest,
                 const std::function<bool(uint64_t, uint64_t)>& progress, std::string* error);

} /* namespace unzip */
