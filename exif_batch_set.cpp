#include <exiv2/exiv2.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static bool IsValidExifDateTime(const std::string& s) {
    // "YYYY:MM:DD HH:MM:SS" => 19 chars
    if (s.size() != 19) return false;
    auto is_digit = [](char c) { return (c >= '0' && c <= '9'); };

    for (int i : {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18}) {
        if (!is_digit(s[static_cast<size_t>(i)])) return false;
    }
    return (s[4] == ':' && s[7] == ':' && s[10] == ' ' && s[13] == ':' && s[16] == ':');
}

static std::string ToLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool IsJpegPath(const fs::path& p) {
    const auto ext = ToLower(p.extension().string());
    return (ext == ".jpg" || ext == ".jpeg");
}

static void PrintUsage() {
    std::cerr
        << "Usage:\n"
        << "  exif_batch_set.exe <folder> \"YYYY:MM:DD HH:MM:SS\" [options]\n"
        << "\nOptions:\n"
        << "  --recursive     Process subfolders\n"
        << "  --dry-run       Do not modify files, just print what would be changed\n"
        << "  --no-backup     Do not create .bak backup files\n"
        << "\nExamples:\n"
        << "  exif_batch_set.exe C:\\\\photos \"2026:02:25 18:30:00\"\n"
        << "  exif_batch_set.exe C:\\\\photos \"2026:02:25 18:30:00\" --recursive\n"
        << "  exif_batch_set.exe C:\\\\photos \"2026:02:25 18:30:00\" --dry-run\n";
}

static bool CopyFileBinary(const fs::path& src, const fs::path& dst, std::string* err) {
    std::ifstream in(src, std::ios::binary);
    if (!in) {
        if (err) *err = "failed to open source for backup";
        return false;
    }
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (err) *err = "failed to open destination for backup";
        return false;
    }
    out << in.rdbuf();
    if (!out.good()) {
        if (err) *err = "failed while writing backup";
        return false;
    }
    return true;
}

static bool UpdateExifInPlace(const fs::path& file,
                             const std::string& dt,
                             bool dry_run,
                             bool make_backup) {
    try {
        if (dry_run) {
            std::cout << "DRY: " << file << "\n";
            return true;
        }

        fs::path backup_path = file;
        backup_path += ".bak";

        if (make_backup) {
            // Do not overwrite existing backup silently.
            if (fs::exists(backup_path)) {
                std::cerr << "ERR: backup already exists, skip: " << backup_path << "\n";
                return false;
            }
            std::string err;
            if (!CopyFileBinary(file, backup_path, &err)) {
                std::cerr << "ERR: backup failed: " << file << " : " << err << "\n";
                return false;
            }
        }

        auto image = Exiv2::ImageFactory::open(file.string());
        if (!image.get()) {
            std::cerr << "ERR: open failed: " << file << "\n";
            return false;
        }

        image->readMetadata();
        Exiv2::ExifData& exif = image->exifData();

        // Main “date taken” tags
        exif["Exif.Photo.DateTimeOriginal"] = dt;   // 0x9003
        exif["Exif.Photo.DateTimeDigitized"] = dt;  // 0x9004
        exif["Exif.Image.DateTime"] = dt;           // 0x0132 (ModifyDate)

        image->setExifData(exif);
        image->writeMetadata();

        std::cout << "OK : " << file << "\n";
        return true;

    } catch (const Exiv2::Error& e) {
        std::cerr << "ERR: " << file << " : " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "ERR: " << file << " : " << e.what() << "\n";
        return false;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        PrintUsage();
        return 2;
    }

    const fs::path folder = argv[1];
    const std::string dt = argv[2];

    bool recursive = false;
    bool dry_run = false;
    bool make_backup = true;

    for (int i = 3; i < argc; ++i) {
        const std::string opt = argv[i];
        if (opt == "--recursive") {
            recursive = true;
        } else if (opt == "--dry-run") {
            dry_run = true;
        } else if (opt == "--no-backup") {
            make_backup = false;
        } else {
            std::cerr << "Unknown option: " << opt << "\n";
            PrintUsage();
            return 2;
        }
    }

    if (!IsValidExifDateTime(dt)) {
        std::cerr << "Invalid datetime. Expected: \"YYYY:MM:DD HH:MM:SS\"\n";
        return 2;
    }

    std::error_code ec;
    if (!fs::exists(folder, ec) || !fs::is_directory(folder, ec)) {
        std::cerr << "Folder does not exist or is not a directory: " << folder << "\n";
        return 2;
    }

    std::size_t total = 0;
    std::size_t ok = 0;
    std::size_t skipped = 0;

    auto process_entry = [&](const fs::directory_entry& e) {
        if (!e.is_regular_file()) return;
        const auto& p = e.path();
        if (!IsJpegPath(p)) { ++skipped; return; }

        ++total;
        if (UpdateExifInPlace(p, dt, dry_run, make_backup)) ++ok;
    };

    if (recursive) {
        for (const auto& e : fs::recursive_directory_iterator(folder)) {
            process_entry(e);
        }
    } else {
        for (const auto& e : fs::directory_iterator(folder)) {
            process_entry(e);
        }
    }

    std::cout << "Done. Updated " << ok << " / " << total
              << " JPEG files. Skipped(non-jpeg): " << skipped << "\n";

    return (ok == total ? 0 : 1);
}
