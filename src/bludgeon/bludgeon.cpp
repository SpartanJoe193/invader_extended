// SPDX-License-Identifier: GPL-3.0-only

#include <vector>
#include <string>
#include <filesystem>
#include <invader/printf.hpp>
#include <invader/version.hpp>
#include <invader/tag/hek/header.hpp>
#include <invader/tag/hek/definition.hpp>
#include <invader/command_line_option.hpp>
#include <invader/tag/parser/parser.hpp>
#include <invader/file/file.hpp>

#include "bludgeoner.hpp"

enum WaysToFuckUpTheTag : std::uint64_t {
    /** Apply no fixes; just see what we can do */
    NO_FIXES                            = 0,

    /** Fix color change in tag being incorrect */
    INVALID_COLOR_CHANGE                = 1ull << 0,

    /** Fix invalid enums being used (this is present in stock HEK tags, and in some cases with HEK+ extracted tags,
        this has led to the game crashing) */
    BROKEN_ENUMS                        = 1ull << 1,

    /** Fix bullshit tag references being used (e.g. light tags being referenced in models) */
    BROKEN_REFERENCE_CLASSES            = 1ull << 2,

    /** Fix the incorrect sound format being reported in the header (incorrect format was done to work around Halo not
        allowing 16-bit PCM, but this was fixed in mods, and this is undefined behavior) */
    BROKEN_SOUND_FORMAT                 = 1ull << 3,

    /** Fix strings not being null terminated or being the wrong length */
    BROKEN_STRINGS                      = 1ull << 4,

    /** Extract scripts (not having this results in undefined behavior when built by tool.exe) */
    MISSING_SCRIPTS                     = 1ull << 5,

    /** Fix an incorrect sound buffer size */
    INVALID_SOUND_BUFFER                = 1ull << 6,
    
    /** Fix invalid values that are out of bounds for their ranges */
    BROKEN_RANGE                        = 1ull << 7,
    
    /** Fix indices that are out of bounds */
    INVALID_INDICES                     = 1ull << 8,

    /** Fix normals (broken normals crashes tool.exe and sapien when generating lightmaps) */
    INVALID_NORMALS                     = 1ull << 9,
    
    /** Fix model markers not being put in the right place (not having this results in undefined behavior when built by
        tool.exe) */
    INVALID_MODEL_MARKERS               = 1ull << 10,

    /** Regenerate missing compressed/uncompressed vertices (not having these fucks up lightmap generation) */
    INVALID_VERTICES                    = 1ull << 11,

    /** Fix sound permutations not being valid (caused by old versions of Refinery when safe mode is enabled) */
    INVALID_SOUND_PERMUTATIONS          = 1ull << 12,
    
    /** Attempt to unfuck anything that can be unfucked (CAUTION: you can unscrew a lightbulb; you can't unscrew a Halo tag) */
    EVERYTHING                          = ~0ull
};

#define NO_FIXES_FIX "none"
#define INVALID_COLOR_CHANGE_FIX "broken-color-change"
#define BROKEN_ENUMS_FIX "invalid-enums"
#define BROKEN_REFERENCE_CLASSES_FIX "invalid-reference-classes"
#define BROKEN_SOUND_FORMAT_FIX "invalid-sound-format"
#define INVALID_MODEL_MARKERS_FIX "invalid-model-markers"
#define INVALID_VERTICES_FIX "missing-vertices"
#define MISSING_SCRIPTS_FIX "missing-script-source"
#define BROKEN_RANGE_FIX "out-of-range"
#define INVALID_SOUND_PERMUTATIONS_FIX "invalid-sound-permutations"
#define INVALID_SOUND_BUFFER_FIX "incorrect-sound-buffer"
#define INVALID_INDICES_FIX "invalid-indices"
#define INVALID_NORMALS_FIX "nonnormal-vectors"
#define BROKEN_STRINGS_FIX "invalid-strings"
#define EVERYTHING_FIX "everything"

static int bludgeon_tag(const char *file_path, std::uint64_t fixes, bool &bludgeoned) {
    using namespace Invader::Bludgeoner;
    using namespace Invader::HEK;
    using namespace Invader::File;

    bludgeoned = false;

    // Open the tag
    auto tag = open_file(file_path);
    if(!tag.has_value()) {
        eprintf_error("Failed to open %s", file_path);
        return EXIT_FAILURE;
    }

    // Get the header
    std::vector<std::byte> file_data;
    try {
        const auto *header = reinterpret_cast<const TagFileHeader *>(tag->data());
        Invader::HEK::TagFileHeader::validate_header(header, tag->size());
        auto parsed_data = Invader::Parser::ParserStruct::parse_hek_tag_file(tag->data(), tag->size());

        // No fixes; try to detect things
        bool issues_present = false;
        if(fixes == WaysToFuckUpTheTag::NO_FIXES) {
            #define check_fix(fix, fix_message) if(fix(parsed_data.get(), false)) { \
                oprintf_success_warn("%s: " fix_message, file_path); \
                issues_present = true; \
            }
            
            check_fix(broken_enums, "invalid enums detected; fix with " BROKEN_ENUMS_FIX);
            check_fix(broken_references, "invalid reference class detected; fix with " BROKEN_REFERENCE_CLASSES_FIX);
            check_fix(invalid_model_markers, "invalid model markers detected; fix with " INVALID_MODEL_MARKERS_FIX);
            check_fix(sound_buffer, "incorrect sound buffer size on one or more permutations; fix with " INVALID_SOUND_BUFFER_FIX);
            check_fix(broken_vertices, "missing compressed or uncompressed vertices; fix with " INVALID_VERTICES_FIX);
            check_fix(broken_range_fix, "value(s) are out of range; fix with " BROKEN_RANGE_FIX);
            check_fix(missing_scripts, "script source data is missing; fix with " MISSING_SCRIPTS_FIX);
            check_fix(broken_indices_fix, "indices are out of bounds; fix with " INVALID_INDICES_FIX);
            check_fix(broken_normals, "problematic nonnormal vectors detected; fix with " INVALID_NORMALS_FIX);
            check_fix(broken_strings, "problematic strings detected; fix with " BROKEN_STRINGS_FIX);
            
            #undef check_fix
        }
        else {
            #define apply_fix(fix, fix_enum, fix_name) if((fixes & fix_enum) && fix(parsed_data.get(), true)) { \
                oprintf_success("%s: Fixed " fix_name, file_path); \
                issues_present = true; \
            }
            
            apply_fix(broken_enums, BROKEN_ENUMS, BROKEN_ENUMS_FIX);
            apply_fix(broken_references, BROKEN_REFERENCE_CLASSES, BROKEN_REFERENCE_CLASSES_FIX);
            apply_fix(invalid_model_markers, INVALID_MODEL_MARKERS, INVALID_MODEL_MARKERS_FIX);
            apply_fix(broken_range_fix, BROKEN_RANGE, BROKEN_RANGE_FIX);
            apply_fix(sound_buffer, INVALID_SOUND_BUFFER, INVALID_SOUND_BUFFER_FIX);
            apply_fix(broken_vertices, INVALID_VERTICES, INVALID_VERTICES_FIX);
            apply_fix(missing_scripts, MISSING_SCRIPTS, MISSING_SCRIPTS_FIX);
            apply_fix(broken_indices_fix, INVALID_INDICES, INVALID_INDICES_FIX);
            apply_fix(broken_normals, INVALID_NORMALS, INVALID_NORMALS_FIX);
            apply_fix(broken_strings, BROKEN_STRINGS, BROKEN_STRINGS_FIX);
            
            #undef apply_fix
        }

        // No issues? OK
        if(!issues_present) {
            return EXIT_SUCCESS;
        }
        
        bludgeoned = true;

        // Exit out of here
        if(fixes == WaysToFuckUpTheTag::NO_FIXES) {
            return EXIT_SUCCESS;
        }

        // Do it!
        file_data = parsed_data->generate_hek_tag_data(header->tag_class_int, true);
        if(!Invader::File::save_file(file_path, file_data)) {
            eprintf_error("Error: Failed to write to %s.", file_path);
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
    catch(std::exception &e) {
        eprintf_error("Error: Failed to bludgeon %s: %s", file_path, e.what());
        return EXIT_FAILURE;
    }
}

int main(int argc, char * const *argv) {
    EXIT_IF_INVADER_EXTRACT_HIDDEN_VALUES

    std::vector<Invader::CommandLineOption> options;
    options.emplace_back("info", 'i', 0, "Show license and credits.");
    options.emplace_back("tags", 't', 1, "Use the specified tags directory.", "<dir>");
    options.emplace_back("fs-path", 'P', 0, "Use a filesystem path for the tag path if specifying a tag.");
    options.emplace_back("all", 'a', 0, "Bludgeon all tags in the tags directory.");
    options.emplace_back("type", 'T', 1, "Type of bludgeoning. Can be: " BROKEN_ENUMS_FIX ", " BROKEN_RANGE_FIX ", " BROKEN_STRINGS_FIX ", " BROKEN_REFERENCE_CLASSES_FIX ", " INVALID_MODEL_MARKERS_FIX ", " MISSING_SCRIPTS_FIX ", " INVALID_SOUND_BUFFER_FIX ", " INVALID_VERTICES_FIX ", " INVALID_NORMALS_FIX ", " NO_FIXES_FIX ", " EVERYTHING_FIX " (default: " NO_FIXES_FIX ")");

    static constexpr char DESCRIPTION[] = "Convinces tags to work with Invader.";
    static constexpr char USAGE[] = "[options] <-a | tag.class>";

    struct BludgeonOptions {
        const char *tags = "tags";
        bool use_filesystem_path = false;
        bool all = false;
        std::uint64_t fixes = WaysToFuckUpTheTag::NO_FIXES;
    } bludgeon_options;

    auto remaining_arguments = Invader::CommandLineOption::parse_arguments<BludgeonOptions &>(argc, argv, options, USAGE, DESCRIPTION, 0, 1, bludgeon_options, [](char opt, const std::vector<const char *> &arguments, auto &bludgeon_options) {
        switch(opt) {
            case 't':
                bludgeon_options.tags = arguments[0];
                break;
            case 'i':
                Invader::show_version_info();
                std::exit(EXIT_SUCCESS);
            case 'P':
                bludgeon_options.use_filesystem_path = true;
                break;
            case 'a':
                bludgeon_options.all = true;
                break;
            case 'T':
                if(std::strcmp(arguments[0], NO_FIXES_FIX) == 0) {
                    bludgeon_options.fixes = WaysToFuckUpTheTag::NO_FIXES;
                }
                else if(std::strcmp(arguments[0], INVALID_COLOR_CHANGE_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::INVALID_COLOR_CHANGE;
                }
                else if(std::strcmp(arguments[0], BROKEN_ENUMS_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::BROKEN_ENUMS;
                }
                else if(std::strcmp(arguments[0], BROKEN_REFERENCE_CLASSES_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::BROKEN_REFERENCE_CLASSES;
                }
                else if(std::strcmp(arguments[0], MISSING_SCRIPTS_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::MISSING_SCRIPTS;
                }
                else if(std::strcmp(arguments[0], BROKEN_SOUND_FORMAT_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::BROKEN_SOUND_FORMAT;
                }
                else if(std::strcmp(arguments[0], INVALID_MODEL_MARKERS_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::INVALID_MODEL_MARKERS;
                }
                else if(std::strcmp(arguments[0], INVALID_VERTICES_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::INVALID_VERTICES;
                }
                else if(std::strcmp(arguments[0], INVALID_SOUND_PERMUTATIONS_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::INVALID_SOUND_PERMUTATIONS;
                }
                else if(std::strcmp(arguments[0], INVALID_SOUND_BUFFER_FIX) == 0) {
                    #ifndef DISABLE_AUDIO
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::INVALID_SOUND_BUFFER;
                    #else
                    eprintf_error("invader was not compiled with audio support, so " INVALID_SOUND_BUFFER_FIX " is not available");
                    std::exit(EXIT_FAILURE);
                    #endif
                }
                else if(std::strcmp(arguments[0], BROKEN_RANGE_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::BROKEN_RANGE;
                }
                else if(std::strcmp(arguments[0], INVALID_INDICES_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::INVALID_INDICES;
                }
                else if(std::strcmp(arguments[0], INVALID_NORMALS_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::INVALID_NORMALS;
                }
                else if(std::strcmp(arguments[0], BROKEN_STRINGS_FIX) == 0) {
                    bludgeon_options.fixes = bludgeon_options.fixes | WaysToFuckUpTheTag::BROKEN_STRINGS;
                }
                else if(std::strcmp(arguments[0], EVERYTHING_FIX) == 0) {
                    bludgeon_options.fixes = WaysToFuckUpTheTag::EVERYTHING;
                }
                else {
                    eprintf_error("Unknown fix type %s", arguments[0]);
                    std::exit(EXIT_FAILURE);
                }
                break;
        }
    });

    auto &fixes = bludgeon_options.fixes;

    if(remaining_arguments.size() == 0) {
        if(!bludgeon_options.all) {
            eprintf_error("Expected --all to be used OR a tag path. Use -h for more information.");
            return EXIT_FAILURE;
        }

        std::size_t success = 0;
        std::size_t total = 0;

        auto recursively_bludgeon_dir = [&total, &success, &fixes](const std::filesystem::path &dir, auto &recursively_bludgeon_dir) -> void {
            for(auto i : std::filesystem::directory_iterator(dir)) {
                if(i.is_directory()) {
                    recursively_bludgeon_dir(i, recursively_bludgeon_dir);
                }
                else if(i.is_regular_file()) {
                    total++;
                    bool bludgeoned;
                    bludgeon_tag(i.path().string().c_str(), fixes, bludgeoned);
                    success += bludgeoned;
                }
            }
        };

        recursively_bludgeon_dir(std::filesystem::path(bludgeon_options.tags), recursively_bludgeon_dir);

        oprintf("Bludgeoned %zu out of %zu tag%s\n", success, total, total == 1 ? "" : "s");

        return EXIT_SUCCESS;
    }
    else if(bludgeon_options.all) {
        eprintf_error("--all and a tag path cannot be used at the same time. Use -h for more information.");
        return EXIT_FAILURE;
    }
    else {
        std::filesystem::path file_path;
        if(bludgeon_options.use_filesystem_path) {
            file_path = std::string(remaining_arguments[0]);
        }
        else {
            file_path = std::filesystem::path(bludgeon_options.tags) / Invader::File::halo_path_to_preferred_path(remaining_arguments[0]);
        }
        std::string file_path_str = file_path.string();
        bool bludgeoned;
        int result = bludgeon_tag(file_path_str.c_str(), fixes, bludgeoned);
        if(result == EXIT_SUCCESS && !bludgeoned) {
            oprintf("%s: No issues detected\n", file_path_str.c_str());
        }
        return result;
    }
}
