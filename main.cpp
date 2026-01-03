#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <fstream>
#include <unordered_map>

namespace fs = std::filesystem;

static const std::string TMP_BASE = "/tmp/xpkgm";
// Comments explain the working of this code
/* ---------------- TOML Parser (work in progress) ---------------- */

std::unordered_map<std::string, std::string>
readToml(const fs::path& path) {
    std::unordered_map<std::string, std::string> data;
    std::ifstream file(path);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // trim key
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);

        // trim value and quotes
        val.erase(0, val.find_first_not_of(" \t\""));
        val.erase(val.find_last_not_of(" \t\"") + 1);

        data[key] = val;
    }
    return data;
}

/* ---------------- Installation funcs and other stuff ---------------- */

void installPackage(const std::string& pkgPath) {
    if (!fs::exists(pkgPath)) {
        std::cerr << "Package does not exist!\n";
        return;
    }

    fs::create_directories(TMP_BASE);
    fs::path workDir = fs::path(TMP_BASE) / "install";
    fs::remove_all(workDir);
    fs::create_directories(workDir);

    // Extract .xpkg (cpio)
    std::string cpioCmd =
        "cd " + workDir.string() +
        " && cpio -id < " + fs::absolute(pkgPath).string();

    if (system(cpioCmd.c_str()) != 0) {
        std::cerr << "Failed to extract xpkg.\n";
        return;
    }

    // Metadata required
    fs::path metaPath = workDir / "xpkg.toml";
    if (!fs::exists(metaPath)) {
        std::cerr << "xpkg.toml missing!\n";
        return;
    }

    auto meta = readToml(metaPath);

    std::cout << "Installing package:\n";
    std::cout << "  Name: " << meta["name"] << "\n";
    std::cout << "  Version: " << meta["version"] << "\n";
    std::cout << "  Description: " << meta["description"] << "\n";

    fs::path payload = workDir / "payload.tar.gz";
    if (!fs::exists(payload)) {
        std::cerr << "payload.tar.gz missing!\n";
        return;
    }

    // Extract payload
    std::string tarCmd =
        "tar -xzf " + payload.string() + " -C " + workDir.string();

    if (system(tarCmd.c_str()) != 0) {
        std::cerr << "Failed to extract payload.\n";
        return;
    }

    fs::path installScript = workDir / "install.sh";
    if (!fs::exists(installScript)) {
        std::cerr << "install.sh not found!\n";
        return;
    }

    // Run installer
    std::string runCmd = "sh " + installScript.string();
    if (system(runCmd.c_str()) != 0) {
        std::cerr << "install.sh failed.\n";
        return;
    }

    std::cout << "Package installed successfully!\n";
    fs::remove_all(workDir);
}

/* ---------------- Building packages  ---------------- */

void makePackage(const std::string& dirPath) {
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "Invalid directory!\n";
        return;
    }

    fs::path installScript = fs::path(dirPath) / "install.sh";
    fs::path metaPath = fs::path(dirPath) / "xpkg.toml";

    if (!fs::exists(installScript)) {
        std::cerr << "install.sh missing!\n";
        return;
    }

    if (!fs::exists(metaPath)) {
        std::cerr << "xpkg.toml missing!\n";
        return;
    }

    fs::create_directories(TMP_BASE);
    fs::path workDir = fs::path(TMP_BASE) / "build";
    fs::remove_all(workDir);
    fs::create_directories(workDir);

    // Create payload.tar.gz
    fs::path payload = workDir / "payload.tar.gz";
    std::string tarCmd =
        "tar -czf " + payload.string() + " -C " + dirPath + " .";

    if (system(tarCmd.c_str()) != 0) {
        std::cerr << "Failed to create payload.tar.gz\n";
        return;
    }

    // Copy metadata into build dir
    fs::copy_file(metaPath, workDir / "xpkg.toml",
                  fs::copy_options::overwrite_existing);

    // Create .xpkg (cpio)
    fs::path pkgName = fs::path(dirPath).filename();
    pkgName += ".tar.gz.xpkg";

    std::string cpioCmd =
        "cd " + workDir.string() +
        " && printf \"payload.tar.gz\nxpkg.toml\n\" | "
        "cpio -o -H newc > " + fs::absolute(pkgName).string();

    if (system(cpioCmd.c_str()) != 0) {
        std::cerr << "Failed to create xpkg.\n";
        return;
    }

    std::cout << "Package created: " << pkgName << "\n";
    fs::remove_all(workDir);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout <<
            "Usage:\n"
            "  xpm -F <package.tar.gz.xpkg>    Install package\n"
            "  xpm --make-pkg <directory>     Create package\n";
        return 0;
    }

    std::string arg = argv[1];
    if (arg == "-F" && argc >= 3) {
        installPackage(argv[2]);
    } else if (arg == "--make-pkg" && argc >= 3) {
        makePackage(argv[2]);
    } else {
        std::cerr << "Invalid arguments\n";
        return 1;
    }

    return 0;
}

