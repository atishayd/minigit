#include <iostream>
#include <filesystem>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <unordered_map>

namespace fs = std::filesystem;

struct FileInfo {
	uintmax_t size;
	std::time_t modified;
};

std::time_t filetime_to_time_t(fs::file_time_type ft) {
	using namespace std::chrono;

	auto sctp = time_point_cast <system_clock::duration>(ft - fs::file_time_type::clock::now() + system_clock::now());
	return system_clock::to_time_t(sctp);
}

std::unordered_map<std::string, FileInfo>
load_previous_state(const fs::path& state_file) {
	std::unordered_map<std::string, FileInfo> state;
	
	std::ifstream in(state_file);
	if (!in) {
		return state;
	}

	std::string line;
	while (std::getline(in, line)) {
		std::istringstream iss(line);
		std::string path, size_str, time_str;

		if (std::getline(iss, path, '|') && std::getline(iss, size_str, '|') && std::getline(iss, time_str)) {
			FileInfo info;
			info.size = std::stoull(size_str);
			info.modified = static_cast<std::time_t>(std::stoll(time_str));
			state[path] = info;
		}
	}
	return state;
}

void save_state(const fs::path& state_file, const std::unordered_map<std::string, FileInfo>& state) {
	std::ofstream out(state_file);
	for (const auto& [path, info] : state) {
		out << path << "|" << info.size << "|" << info.modified << "\n";
	}
}


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

	fs::path state_dir = target_root / ".backup_state";
	fs::create_directories(state_dir);
	fs::path state_file = state_dir / "last_state.txt";
	
	// load prev state
	auto previous_state = load_previous_state(state_file);

	// track new state
	std::unordered_map<std::string, FileInfo> current_state;

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
			fs::path rel = fs::relative(src_path, source);
			std::string rel_str = rel.string();

			uintmax_t size = entry.file_size();
			auto ftime = entry.last_write_time();
			std::time_t modified = filetime_to_time_t(ftime);

			current_state[rel_str] = {size, modified};

			auto it = previous_state.find(rel_str);
			if (it != previous_state.end()) {
				if (it->second.size == size && it->second.modified == modified) {
					manifest << "SKIP (unchanged) " << rel_str << "\n";
					continue;
				}
			}

			fs::create_directories(dst_path.parent_path(), ec);
			fs::copy_file(src_path, dst_path, fs::copy_options::overwrite_existing, ec);

			if (ec) {
				++copy_errors;
				manifest << "ERROR copy: " << rel_str << " (" << ec.message() << ")\n";
				ec.clear();
			} else {
				++files_copied;
				manifest << "FILE " << rel_str << "\n";
			}
		}
	}
	manifest << "\n--- Summary ---\n";
	manifest << "dirs_created " << dirs_created << "\n";
	manifest << "files_copied " << files_copied << "\n";
	manifest << "errors: " << copy_errors << "\n";

	std::cout << "Backup complete!\n";
	std::cout << " backup_dir: " << backup_dir << "\n";
	std::cout << " manifest: " << manifest_path << "\n";
	std::cout << " files: " << files_copied << "\n";
	std::cout << " errors: " << copy_errors << "\n";

	save_state(state_file, current_state);
	return (copy_errors ==0) ? 0 : 2;
}
