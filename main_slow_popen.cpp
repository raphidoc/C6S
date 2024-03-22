#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <jsoncpp/json/json.h>
#include <netcdf.h>
#include <chrono>
#include <thread>
#include <vector>
#include <sstream>

using namespace std;

std::vector<float> create_range(float min, float max, float step) {
    int size = ((max - min) / step) + 1;
    std::vector<float> range(size);

    for(int i = 0; i < size; i++) {
        range[i] = min + (i * step);
    }

    return range;
}

void cartesianProduct(std::vector<std::vector<float>>& dimensions, std::vector<std::vector<float>>& result, std::vector<float> temp, int dimensionIndex) {
    if (dimensionIndex == dimensions.size()) {
        // Use emplace_back instead of push_back
        result.emplace_back(temp);
    } else {
        for (float value : dimensions[dimensionIndex]) {
            temp[dimensionIndex] = value;
            cartesianProduct(dimensions, result, temp, dimensionIndex + 1);
        }
    }
}

std::pair<std::string, int> exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    auto pipeDeleter = [](FILE* p) { if (p) fclose(p); };
    std::unique_ptr<FILE, decltype(pipeDeleter)> pipe(popen(cmd, "r"), pipeDeleter);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    int exitCode = pclose(pipe.release());
    return std::make_pair(result, exitCode);
}

std::string formatEstimatedTime(float estimatedTime) {
    std::ostringstream oss;
    if (estimatedTime >= 60.0 * 60 * 24 * 365 * 100) {
        oss << estimatedTime / (60.0 * 60 * 24 * 365 * 100) << " centuries";
    } else if (estimatedTime >= 60.0 * 60 * 24 * 365) {
        oss << estimatedTime / (60.0 * 60 * 24 * 365) << " years";
    } else if (estimatedTime >= 60 * 60 * 24 * 30) {
        oss << estimatedTime / (60 * 60 * 24 * 30) << " months";
    } else if (estimatedTime >= 60 * 60 * 24 * 7) {
        oss << estimatedTime / (60 * 60 * 24 * 7) << " weeks";
    } else if (estimatedTime >= 60 * 60 * 24) {
        oss << estimatedTime / (60 * 60 * 24) << " days";
    } else if (estimatedTime >= 60 * 60) {
        oss << estimatedTime / (60 * 60) << " hours";
    } else if (estimatedTime >= 60) {
        oss << estimatedTime / 60 << " minutes";
    } else {
        oss << estimatedTime << " seconds";
    }
    return oss.str();
}

#define ERRCODE 2
#define ERR(e) {printf("Error: %s\n", nc_strerror(e)); exit(ERRCODE);}

int main() {

    char executable[] = "/home/raphael/CLionProjects/C6S/6sV2.1/sixsV2.1";

    // GOOD DIMENSION
    std::vector<std::vector<float>> dimensions = {
            create_range(0, 80, 10), // sun zenith
            create_range(0, 80, 10), // view zenith
            create_range(0, 180, 20), // relative azimuth
            {0.0f, 0.001f, 0.01f, 0.02f, 0.05f, 0.1f, 0.15f,
             0.2f, 0.3f, 0.5f, 0.7f, 1.0f, 1.3f, 1.6f,
             2.0f, 3.0f, 5.0f}, // taer550
            {750.0f, 1013.0f, 1100.0f}, // pressure at target
            {-1, -3}, // sensor altitude
            create_range(0.340f, 1.0f, 0.01f) // wavelength
    };

    // TEST DIMENSION
//    std::vector<std::vector<float>> dimensions = {
//            create_range(0, 80, 40), // sun zenith
//            create_range(0, 80, 40), // view zenith
//            create_range(0, 180, 30), // relative azimuth
//            {0.0f, 3.0f}, // taer550
//            {1013.0f}, // pressure at target
//            {-1}, // sensor altitude
//            create_range(0.340f, 1.0f, 0.1f) // wavelength
//    };

    std::vector<std::vector<float>> combination;
    std::vector<float> temp(dimensions.size());
    cartesianProduct(dimensions, combination, temp, 0);

//        for (const auto& res : combination) {
//            for (float val : res) {
//                std::cout << val << ' ';
//            }
//            std::cout << std::endl;
//        }

    printf("number of combination: %i \n", combination.size());

    std::vector<float> atmospheric_radiance_at_sensor_values(combination.size());

    // Create a vector to hold the commands
    std::vector<std::string> commands(combination.size());

    // Get starting time
    auto start1 = std::chrono::high_resolution_clock::now();
    auto lastOutputTime1 = start1;

    for (int i = 0; i < combination.size(); ++i) {

        char command[1000];
        sprintf(command,
                "echo \"\n"
                "0 # IGEOM\n"
                "%f 0.0 %f %f 1 1 #sun_zenith sun_azimuth view_zenith view_azimuth month day\n"
                "0 # IDATM no gas\n"
                "2 # IAER maritime\n"
                "0 # visibility\n"
                "%f # taer550\n"
                "%f # XPS pressure at terget\n"
                "%f # XPP sensor altitude\n"
                "-1.0 -1.0 # UH20 UO3 below sensor\n"
                "-1.0 # taer550 below sensor\n"
                "-1 # IWAVE monochromatic\n"
                "%f # wavelength\n"
                "0 # INHOMO\n"
                "0 # IDIREC\n"
                "0 # IGROUN 0 = rho\n"
                "0 # surface reflectance\n"
                "-1 # IRAPP no atmospheric correction\n"
                "\" | %s",
                combination[i][0],
                combination[i][1],
                combination[i][2],
                combination[i][3],
                combination[i][4],
                combination[i][5],
                combination[i][6],
                executable
        );


        //cout << command << std::endl;
        //cout << i << std::endl;

        commands[i] = command;

        auto now = std::chrono::high_resolution_clock::now();
        // Calculate the elapsed time since the last output
        auto timeSinceLastOutput = std::chrono::duration_cast<std::chrono::seconds>(now - lastOutputTime1).count();

        if (timeSinceLastOutput >= 1) { // To avoid division by zero

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start1).count();

            printf("\r(%i/%i) | ", static_cast<int>(i), combination.size());

            float iterations_per_second = static_cast<float>(i) / elapsed;
            std::cout << "Iterations per second: " << iterations_per_second;

            float estimated_total_time = (combination.size() - i) / iterations_per_second;
            std::cout << " | Estimated time upon completion: " << formatEstimatedTime(estimated_total_time) << std::flush;

            // Update the last output time
            lastOutputTime1 = now;

        }
    }

    // Get starting time
    auto start2 = std::chrono::high_resolution_clock::now();
    auto lastOutputTime2 = start2;

    for (int i = 6295; i < commands.size(); i++) {

        auto result = exec(commands[i].c_str());
//        std::string jsonOutput = result.first;
//        int exitCode = result.second;
//
//        //cout << exitCode << "\n";
//        if (exitCode != 0) {
//            printf("Error on iteration: %i\n", i);
//            std::cout << commands[i] << std::endl;
//            throw std::runtime_error("Command failed with exit code: " + std::to_string(exitCode));
//        }
//
//        //std::cout << jsonOutput << std::endl;
//
//        Json::Value root;
//        std::string errs;
//        Json::CharReaderBuilder builder;
//        Json::CharReader *reader = builder.newCharReader();
//
//        if (!reader->parse(jsonOutput.c_str(), jsonOutput.c_str() + jsonOutput.size(), &root, &errs)) {
//            std::cout << errs << std::endl;
//            //return EXIT_FAILURE;
//        }
//
//        // Get the type of the root JSON value
//        Json::ValueType type = root.type();
//        // Convert the type to a string and print it
//        std::string typeStr;
//        switch (type) {
//            case Json::nullValue:
//                typeStr = "nullValue";
//                break;
//            case Json::intValue:
//                typeStr = "intValue";
//                break;
//            case Json::uintValue:
//                typeStr = "uintValue";
//                break;
//            case Json::realValue:
//                typeStr = "realValue";
//                break;
//            case Json::stringValue:
//                typeStr = "stringValue";
//                break;
//            case Json::booleanValue:
//                typeStr = "booleanValue";
//                break;
//            case Json::arrayValue:
//                typeStr = "arrayValue";
//                break;
//            case Json::objectValue:
//                typeStr = "objectValue";
//                break;
//            default:
//                typeStr = "unknown type";
//        }
//
//        // Check if root is a JSON object
//        if (root.isObject() && root.isMember("atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]")) {
//            // Check if the key exists in the JSON object
//            if (root.isMember("atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]")) {
//
//                std::string atmospheric_radiance_at_sensor =
//                        root["atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]"].asString();
//
//                atmospheric_radiance_at_sensor_values[i] = std::stof(atmospheric_radiance_at_sensor);
//
//            } else {
//                std::cout
//                        << "Error: Key 'atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]' does not exist in the JSON object"
//                        << std::endl;
//            }
//        } else {
//            std::cout << jsonOutput << std::endl;
//            throw std::runtime_error("Error: root is not a JSON object: " + typeStr);
//        }

        auto now = std::chrono::high_resolution_clock::now();
        // Calculate the elapsed time since the last output
        auto timeSinceLastOutput = std::chrono::duration_cast<std::chrono::seconds>(now - lastOutputTime2).count();

        if (timeSinceLastOutput >= 1) { // To avoid division by zero

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start2).count();

            printf("\r(%i/%i) | ", static_cast<int>(i), combination.size());

            float iterations_per_second = static_cast<float>(i) / elapsed;
            std::cout << "Iterations per second: " << iterations_per_second;

            float estimated_total_time = (combination.size() - i) / iterations_per_second;
            std::cout << " | Estimated time upon completion: " << formatEstimatedTime(estimated_total_time) << std::flush;

            // Update the last output time
            lastOutputTime2 = now;

        }
    }


    /* Always check the return code of every netCDF function call. In
     * this example program, any retval which is not equal to NC_NOERR
     * (0) will cause the program to print an error message and exit
     * with a non-zero return code. */

//    int ncid, dimids[9], varid, retval;
//    int coord_varids[9];
//    size_t start[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
//    size_t count[9] = {static_cast<size_t>(sizes[0]),
//                       static_cast<size_t>(sizes[1]),
//                       static_cast<size_t>(sizes[2]),
//                       static_cast<size_t>(sizes[3]),
//                       static_cast<size_t>(sizes[4]),
//                       static_cast<size_t>(sizes[5]),
//                       static_cast<size_t>(sizes[6]),
//                       static_cast<size_t>(sizes[7]),
//                       static_cast<size_t>(sizes[8])};
//
//    cout << "creating nc file";
//
//    /* Create the file. The NC_CLOBBER parameter tells netCDF to
//     * overwrite this file, if it already exists.*/
//    if ((retval = nc_create("test_rho_path_lut.nc", NC_CLOBBER, &ncid)))
//        ERR(retval);
//
//    cout << "defining dimension";
//
//    // Define the dimensions
//    for (int i = 0; i < 9; i++) {
//        if ((retval = nc_def_dim(ncid, dimension_names[i], sizes[i], &dimids[i])))
//        ERR(retval);
//    }
//
//    cout << "defining data var";
//
//    // Define the variable for atmospheric_radiance_at_sensor
//    if ((retval = nc_def_var(ncid, "atmospheric_radiance_at_sensor", NC_FLOAT, 9, dimids, &varid)))
//        ERR(retval);
//
//    cout << "defining coordinate var";
//
//    // Define the variables for the coordinates
//    for (int i = 0; i < 9; i++) {
//        if ((retval = nc_def_var(ncid, dimension_names[i], NC_FLOAT, 1, &dimids[i], &coord_varids[i])))
//            ERR(retval);
//    }
//
//    cout << "end define mode";
//
//    // End define mode
//    if ((retval = nc_enddef(ncid)))
//        ERR(retval);
//
//    cout << "write data";
//
//    // Write the data for atmospheric_radiance_at_sensor
//    if ((retval = nc_put_vara_float(ncid, varid, start, count, &atmospheric_radiance_at_sensor)))
//        ERR(retval);
//
//    cout << "write coordinate";
//
//    // Write the data for the coordinates
//    for (int i = 0; i < 9; i++) {
//        if ((retval = nc_put_vara_float(ncid, coord_varids[i], &start[i], &count[i], dimensions[i])))
//        ERR(retval);
//    }
//
//    cout << "closing file";
//
//    /* Close the file. This frees up any internal netCDF resources
//     * associated with the file, and flushes any buffers. */
//    if ((retval = nc_close(ncid)))
//        ERR(retval);
//
//    delete reader;

    //cout << returnCode;

    // checking if the command was executed successfully
//    if (returnCode == 0) {
//        cout << "Command executed successfully " << returnCode << endl;
//    } else {
//        cout << "Command execution failed or returned "
//                "non-zero: "
//             << returnCode << endl;
//    }

    return (0);
}