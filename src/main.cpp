#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <source_dir> <target_dir>\n";
		return 1;
	}

	fs::path source = argv[1];
	fs::path target_root = argv[2];

	if (!fs::exists(source)) {
		std::cerr << "Error: source does not exist: " << source << "\n";
		return 1;
	}

	if (!fs::is_directory(source)) {
		std::cerr << "Error: source is not a directory: " << source << "\n";
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

	std::cout << "OK. Ready to backup:\n";
	std::cout << " source " << source << "\n";
	std::cout << " target: " << target_root << "\n";
	return 0;
}
