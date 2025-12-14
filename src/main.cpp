#include <iostream>
#include <filesystem>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace fs = std::filesystem;

std::string timestamp_now() {
	using clock = std::chrono::system_clock;
	auto now = clock::now();
	std::time_t t = clock::to_time_t(now);

	std::tm tm{};
#if defined(_WIN32)
	localtime_s(&tm,&t);
#else
	localtime_r(&t,&tm);
#endif
	
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d_%H%M%S");
	return oss.str();
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <source_dir> <target_dir>\n";
		return 1;
	}

	fs::path source = argv[1];
	fs::path target_root = argv[2];

	if (!fs::exists(source) || !fs::is_directory(source)) {
		std::cerr << "Error: source must be an existing directory: " << source << "\n";
		return 1;
	}

	// ensure root exists

	std::error_code ec;
	fs::create_directories(target_root,ec);
	if (ec) {
		std::cerr << "Error: could not create target directory: " << target_root
			<< " (" << ec.message() << ")\n";
		return 1;
	}

	fs::path backups_dir = target_root / "backups";
	fs::create_directories(backups_dir,ec);
	if (ec) {
		std::cerr << "Error: could not create backups directory: " << backups_dir
			<< " (" << ec.message() << ")\n";
		return 1;
	}

	fs::path backup_dir = backups_dir / timestamp_now();
	fs::create_directories(backup_dir, ec);
	if (ec) {
		std::cerr << "Error: could not create backup directory:  " << backup_dir
			<< " (" << ec.message() << ")\n";
		return 1;
	}

	std::cout << "Backup directory created: " << backup_dir << "\n";
	return 0;

	// manifest log (what we copied)

	fs::path manifest_path = backup_dir / "manifest.txt";
	std::ofstream manifest(manifest_path);
	if (!manifest) {
		std::cerr << "Error: could not open manifest file: " << manifest_path << "\n";
		return 1;
	}

	std::size_t files_copied = 0;
	std::size_t dirs_created = 0;
	std::size_t copy_errors = 0;

	// walk the source dir
	for (const auto& entry : fs ::recursive_directory_iterator(source)) {
		const fs::path&src_path = entry.path();

		// compute relative path inside backup
		fs::path rel = fs::relative(src_path, source, ec);
		if (ec) {
			++copy_errors;
			manifest << "ERROR relative: " << src_path << " (" << ec.message() << ")\n";
			ec.clear();
			continue;
		}

		fs::path dst_path = backup_dir / rel;

		if (entry.is_directory()) {
			fs::create_directories(dst_path, ec);
			if (ec) {
				++copy_errors;
				manifest <<"ERROR mkdir: " << dst_path << " (" << ec.message() << ")\n";
				ec.clear();
			} else {
				++dirs_created;
				manifest << "DIR " << rel.string() << "\n";
			}

		} else if (entry.is_regular_file()) {
			// ensure parent dir exists
			fs::create_directories(dst_path.parent_path(), ec);
			if (ec) {
				++copy_errors;
				manifest << "ERROR mkdir parent: " << dst_path.parent_path()
					<< " (" <<  ec.message() << ")\n";
				ec.clear();
				continue;
			}

			fs::copy_file(src_path, dst_path, fs::copy_options::overwrite_existing, ec);
			if (ec) {
				++copy_errors;
				manifest << "ERROR copy: " << rel.string() << " (" << ec.message() << ")\n";
				ec.clear();
			} else {
				++files_copied;
				manifest << "FILE " << rel.string() << "\n";
			}
		} else {
			//  symlinks / sockets / etc. skipped
			manifest << "SKIP " << rel.string() << "\n";
		}
	}

	manifest << "\n--- SUmmary ---\n";
	manifest << "dirs_created " << dirs_created << "\n";
	manifest << "files_copied " << files_copied << "\n";
	manifest << "errors: " << copy_errors << "\n";

	std::cout << "Backup complete!\n";
	std::cout << " backup_dir: " << backup_dir << "\n";
	std::cout << " manifest: " << manifest_path << "\n";
	std::cout << " files: " << files_copied << "\n";
	std::cout << " errors: " << copy_errors << "\n";

	return (copy_errors ==0) ? 0 : 2;




}
