import posix
import os
import osproc
import strutils
import tables


const TMP_BASE = "/tmp/xpkgm"

## Comments explain the working of this code
# ---------------- TOML Parser (work in progress) ---------------- #

proc readToml(path: string): Table[string, string] =
    var data = initTable[string, string]()
    if fileExists(path):
        for line in lines(path):
            if line.startsWith('#'):
                continue
            let eq = line.find("=")
            if eq == -1:
                continue

            var key = line.substr(0, eq)
            var val = line.substr(eq + 1)

            ## trim key
            key = strip(key, leading = true, trailing = true, {' ', '\t', '='})

            ## trim quotes
            val = strip(val, leading = true, trailing = true, {' ', '\t', '"'})

            data[key] = val

        return data

# ---------------- Installation funcs and other stuff ---------------- #

proc installPackage(pkgPath: string) =
    if not fileExists(pkgPath):
        stderr.write("Package does not exist!\n")
        return

    createDir(TMP_BASE)
    let workDir = TMP_BASE & "/install"
    if dirExists(workDir):
        removeDir(workDir, true)
    else:
        createDir(workDir)

    ## Extract .xpkg (cpio)
    let cpioCmd: string = "cd " & workDir &
    " && cpio -id < " & expandFilename(pkgPath)

    if execCmd(cpioCmd) != 0:
        stderr.write("Failed to extract xpkg.\n")
        return

    ## Metadata required
    let metaPath = workDir & "/xpkg.toml"
    if not fileExists(metaPath):
        stderr.write("xpkg.toml missing!\n")
        return

    let meta = readToml(metaPath)

    stdout.write("Installing package:\n")
    stdout.write("   Name: " & meta["name"] & "\n")
    stdout.write("   Version: " & meta["version"] & "\n")
    stdout.write("   Description: " & meta["description"] & "\n")

    let payload: string = workDir & "/payload.tar.gz"
    if not fileExists(payload):
        stderr.write("payload.tar.gz missing!\n")
        return
    
    ## Extract payload
    let tarCmd: string = "tar -xvf " & payload &
    " -C " & workDir

    if execCmd(tarCmd) != 0:
        stderr.write("Failed to extract payload.\n")
        return

    let installScript: string = workDir & "/install.sh"
    if not fileExists(installScript):
        stderr.write("install.sh not found!\n")
        return
    
    ## Run installer
    let runCmd: string = "sh " & installScript
    if execCmd(runCmd) != 0:
        stderr.write("install.sh failed.\n")
        return

    stdout.write("Package installed successfully!\n")
    removeDir(workDir, true)

# ---------------- Building packages  ---------------- #

proc makePackage(dirPath: string) =
    if not dirExists(dirPath):
        stderr.write("Invalid directory!\n")
        return

    let installScript: string = dirPath & "/install.sh"
    let metaPath: string = dirPath & "/xpkg.toml"

    if not fileExists(installScript):
        stderr.write("install.sh missing!\n")
        return

    if not fileExists(metaPath):
        stderr.write("xpkg.toml missing!\n")
        return

    createDir(TMP_BASE)
    let workDir: string = TMP_BASE & "/build"
    if dirExists(workDir):
        removeDir(workDir, true)
    else:
        createDir(workDir)

    ## Create payload.tar.gz
    let payload: string = workDir & "/payload.tar.gz"
    let tarCmd: string = "tar -czf " & payload &
    " -C " & dirPath & " ."

    if execCmd(tarCmd) != 0:
        stderr.write("Failed to create payload.tar.gz\n")
        return

    ## Copy metadata into build dir
    if fileExists(workDir & "/xpkg.toml"):
        removeFile(workDir & "/xpkg.toml") ## Can't figure out 
                                            ## copyfile() overwrite 
                                            ## option lol :3
    copyFile(metaPath, workDir & "/xpkg.toml")

    ## Create .xpkg (cpio)
    let pkgName: string = dirPath & ".tar.gz.xpkg"

    let cpioCmd: string = "cd " & workDir & 
    " && prinf \"payload.tar.gz\nxpkg.toml\n\" | " &
    "cpio -o -H newc > " & pkgName

    if execCmd(cpioCmd) != 0:
        stderr.write("Failed to create xpkg.\n")
        return

    stdout.write("Package created: " & pkgName & "\n")
    removeDir(workDir, true)

proc main(): int =
    let args = commandLineParams()

    if args.len < 2:
        stdout.write("""
Usage:
    xpm -F <package.tar.gz.xpkg>    Install package
    xpm --make-pkg <directory>     Create package""" & "\n")
        return 0

    if args.len >= 2 and args[0] == "-F":
        installPackage(args[1])
    elif args.len >= 2 and args[0] == "--make-pkg":
        makePackage(args[1])
    else:            
        stderr.write("Invalid arguments\n")
        return 1

    return 0

discard main()