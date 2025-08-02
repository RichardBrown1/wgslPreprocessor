#include <iostream>     // For std::cout, std::cerr
#include <fstream>      // For std::ifstream, std::ofstream
#include <string>       // For std::string, std::getline
#include <filesystem>   // For std::filesystem::path, std::filesystem::absolute, std::filesystem::canonical, etc.
#include <map>          // For std::set to track active includes
#include <vector>
#include <algorithm>

// IMPORTANT: To compile with g++, you might need to use:
// g++ preprocessor.cpp -o preprocessor -std=c++17 -lstdc++fs
// (The -lstdc++fs flag might be needed on some Linux systems for filesystem support)

std::vector<std::filesystem::path> convertActiveIncludesToVector(std::map<std::filesystem::path, uint32_t> map) {
    // 1. Create a vector of pairs (value, key)
    //    We put value (uint32_t) first to easily sort by value.
    //    std::pair<uint32_t, std::filesystem::path>
    std::vector<std::pair<uint32_t, std::filesystem::path>> valueKeyPairs;

    // Populate the vector with pairs from the input map
    for (const auto& pair : map) {
        valueKeyPairs.push_back({pair.second, pair.first}); // {value, key}
    }

    // 2. Sort the vector of pairs by value in descending order
    std::sort(valueKeyPairs.begin(), valueKeyPairs.end(),
              [](const std::pair<uint32_t, std::filesystem::path>& a, const std::pair<uint32_t, std::filesystem::path>& b) {
                  // Sort by value (first element of the pair) in descending order
                  return a.first > b.first;
              });

    // 3. Extract the keys (std::filesystem::path) into a new vector
    std::vector<std::filesystem::path> keysSortedByValueDescending;
    for (const auto& pair : valueKeyPairs) {
        keysSortedByValueDescending.push_back(pair.second); // The original key
    }

    return keysSortedByValueDescending;
}

void removeDot(std::filesystem::path& absoluteIncludedPath) {
    try
    {
        absoluteIncludedPath = std::filesystem::canonical(absoluteIncludedPath);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Error resolving canonical path for included file: " << absoluteIncludedPath << "\r\n" << e.what() << std::endl;
    }
}

/**
 * @brief Preprocesses a text file, handling #include directives with relative path resolution and circular include detection.
 *
 * This function reads the content of a file. If it encounters a line
 * starting with "#include \"<filename>\"", it resolves <filename> relative to
 * currentBaseDir, recursively processes the included file, and inserts its content.
 * It detects and reports circular include dependencies to prevent infinite recursion.
 *
 * @param filePath The absolute path to the file currently being processed.
 * @param currentBaseDir The directory from which relative #include paths within filePath
 * should be resolved.
 * @param outputStream The output stream where the preprocessed content will be written.
 * @param activeIncludes A set tracking the absolute paths of files currently in the include stack.
 * @return True if preprocessing was successful for the given file, false otherwise.
 */
bool findIncludes(const std::filesystem::path &filePath,
                    const std::filesystem::path &currentBaseDir,
                    std::map<std::filesystem::path, uint32_t> &activeIncludes,
                    uint32_t depth )
{ 
    try {
        if(activeIncludes.at(filePath) < depth) {
            activeIncludes.insert_or_assign(filePath, depth);
            return true; //skip finding includes since we have already done it on this file
        }
    }
    catch (std::exception e)
    {
        activeIncludes[filePath] = depth;
    }
    std::ifstream inputFile(filePath); // Open the file using its absolute path
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open file: " << filePath << std::endl;
        activeIncludes.erase(filePath);
        return false;
    }

    std::string line;
    uint32_t nothingFoundCount = 0; //includes should be near the top
    while (std::getline(inputFile, line) && nothingFoundCount < 5)
    {
        const std::string include_directive_prefix = "#include \"";
        if (line.rfind(include_directive_prefix, 0) == 0)
        { // Check if line starts with the prefix
            size_t start_quote_pos = include_directive_prefix.length();
            size_t end_quote_pos = line.find('"', start_quote_pos);

            if (end_quote_pos != std::string::npos)
            {
                std::string includedRelativeFileName = line.substr(start_quote_pos, end_quote_pos - start_quote_pos);
                std::filesystem::path absoluteIncludedPath = currentBaseDir / includedRelativeFileName;
                removeDot(absoluteIncludedPath);
                std::filesystem::path nextBaseDir = absoluteIncludedPath.parent_path();
                if (!findIncludes(absoluteIncludedPath, nextBaseDir, activeIncludes, depth + 1))
                {
                    inputFile.close();
                    activeIncludes.erase(filePath); // Manual cleanup on error
                    return false;
                }
                nothingFoundCount = 0;
            }
            else
            {
                std::cerr << "Warning: Malformed #include directive in " << filePath << ": " << line << std::endl;
            }
        }
        else
        {
            nothingFoundCount++;
        }
    }

    inputFile.close();
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input_file> [output_file]" << std::endl;
        return 1; // Indicate error
    }

    // It's good practice to untie C++ streams from C stdio for performance,
    // though not strictly necessary for correctness here.
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    // 1. Determine the directory of the executable
    std::filesystem::path executablePath = std::filesystem::absolute(argv[0]);
    std::filesystem::path programBaseDir = executablePath.parent_path(); // This is the executable's directory

    // 2. Resolve the input file path relative to the executable's directory
    std::filesystem::path inputFilePathArgument(argv[1]);
    std::filesystem::path absoluteInitialFilePath = programBaseDir / inputFilePathArgument;

    // Normalize the absolute initial file path to remove redundant '.' or '..'
    try
    {
        absoluteInitialFilePath = std::filesystem::canonical(absoluteInitialFilePath);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Error resolving canonical path for initial input file: " << argv[1] << std::endl;
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return 1;
    }

    // The base directory for resolving includes *within* the initial file
    std::filesystem::path initialFileProcessingBaseDir = absoluteInitialFilePath.parent_path();

    // --- Initialize the activeIncludes set ---
    // This set will be passed by reference to all recursive calls.
    std::map<std::filesystem::path, uint32_t> activeIncludes;
    // -----------------------------------------

    //std::cout << "Executable's folder: " << programBaseDir << std::endl;
    //std::cout << "Starting preprocessing for: " << absoluteInitialFilePath << std::endl;
    //std::cout << "Base directory for initial file's includes: " << initialFileProcessingBaseDir << std::endl;

    // Pass the activeIncludes set to the preprocessing function
    if (!findIncludes(absoluteInitialFilePath, initialFileProcessingBaseDir, activeIncludes, 0))
    {
        std::cerr << "findIncludes failed." << std::endl;
    } 
    //else {
    //    std::cout << "findIncludes completed successfully." << std::endl;
    //}

    std::ofstream outputFile;
    std::ostream *outputPtr = &std::cout; // Default to stdout

    if (argc == 3)
    {
        std::string outputFilePathStr = argv[2];
        outputFile.open(outputFilePathStr);
        if (!outputFile.is_open())
        {
            std::cerr << "Error: Could not open output file: " << outputFilePathStr << std::endl;
            return 1;
        }
        outputPtr = &outputFile; // Point to the output file stream
    }

    std::vector<std::filesystem::path> includes = convertActiveIncludesToVector(activeIncludes);

     // TODO Append contents of the files in the includes vector to the outputPtr
    // Iterate through each file path in the 'includes' vector
    for (const auto& filePath : includes)
    {
        std::ifstream inputFile(filePath); // Open the input file
        if (!inputFile.is_open())
        // Check if the input file was opened successfully
        {
            std::cerr << "Error: Could not open input file: " << filePath << std::endl;
            continue; // Skip to the next file if this one can't be opened
        }

        std::string line;
        // Read the input file line by line
        while (std::getline(inputFile, line))
        {
            // Check if the line contains "#include"
            if (line.find("#include") == std::string::npos)
            {
                // If it doesn't contain "#include", write the line to the output stream
                *outputPtr << line << std::endl;
            }
        }

        inputFile.close(); // Close the input file after reading
    }

    if (outputFile.is_open())
    {
        outputFile.close();
    }

    return 0; // Indicate success
}