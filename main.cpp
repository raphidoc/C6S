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

using namespace std;

float* create_range(float min, float max, float step) {
    int size = ((max - min) / step) + 1;
    float* range = (float*)malloc(size * sizeof(float));

    for(int i = 0; i < size; i++) {
        range[i] = min + (i * step);
    }

    return range;
}

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string formatEstimatedTime(float estimatedTime) {
    /* The number of second in a century is 3,153,600,000
     * which is larger than int and cause an overflow.
     * Use a double instead */
    if (estimatedTime >= 60.0 * 60 * 24 * 365 * 100) {
        return std::to_string(estimatedTime / (60.0 * 60 * 24 * 365 * 100)) + " centuries";
    } else if (estimatedTime >= 60.0 * 60 * 24 * 365) {
        return std::to_string(estimatedTime / (60.0 * 60 * 24 * 365)) + " years";
    } else if (estimatedTime >= 60 * 60 * 24 * 30) {
        return std::to_string(estimatedTime / (60 * 60 * 24 * 30)) + " months";
    } else if (estimatedTime >= 60 * 60 * 24 * 7) {
        return std::to_string(estimatedTime / (60 * 60 * 24 * 7)) + " weeks";
    } else if (estimatedTime >= 60 * 60 * 24) {
        return std::to_string(estimatedTime / (60 * 60 * 24)) + " days";
    } else if (estimatedTime >= 60 * 60) {
        return std::to_string(estimatedTime / (60 * 60)) + " hours";
    } else if (estimatedTime >= 60) {
        return std::to_string(estimatedTime / 60) + " minutes";
    } else {
        return std::to_string(estimatedTime) + " seconds";
    }
}

#define ERRCODE 2
#define ERR(e) {printf("Error: %s\n", nc_strerror(e)); exit(ERRCODE);}

int main() {

    char executable[] = "/home/raphael/CLionProjects/C6S/6sV2.1/sixsV2.1";

    // mapping for dimensions in 6S parameters layout
    // index position, parameter name, value meaning
    char params[] = "0\n" // 0, IGEOM, user supplied
                    "0000.000 0000.000 0000.000 000.000 1 1\n" // 2, sun zenith; 11, view zenith; 20, sun azimuth
                    "8\n" // 41, IDATM user
                    "0000.400\n" // 43, UH2O
                    "0000.400\n" // 52, UO3
                    "2\n" // 61, IAER, maritime
                    "0\n" // 63, v, visibility
                    "0000.020\n" // 65, taer550
                    "00000000\n" // 74, XPS pressure at target [mb]
                    "-003.000\n" // 83, XPP sensor altitude relative to target [km]
                    "-1.0 -1.0\n" //92, UH20; , UO3 below sensor
                    "-1.0\n" // 101, taer550 below sensor
                    "-1\n" // 106, IWAVE -1 = monochromatic
                    "0000.350\n" // 109, wavelength
                    "0\n" // 119, INHOMO
                    "0\n" // 121, IDIREC
                    "0\n" // 123, IGROUN 0 = rho
                    "0\n" // 125, Surface reflectance
                    "-1\n"; // 127, IRAPP no atmospheric correction
    //"\"\n";

    int positions[9] = {2, 11, 20, 43, 52, 65, 74, 83, 109}; // dimension starting position in layout

    // dimensions creation
    const char* dimension_names[9] = {
            "sun_zenith",
            "view_zenith",
            "relative_azimuth",
            "water_vapor",
            "ozone",
            "taer550",
            "pressure_at_target",
            "sensor_altitude",
            "wavelength"};

    // GOOD DIMENSION
    float* dimensions[9];
    int sizes[9];
    int indices[9] = {0};

    dimensions[0] = create_range(0, 80, 10); // sun zenith
    sizes[0] = ((80 - 0) / 10) + 1;

    dimensions[1] = create_range(0, 80, 10); // view zenith
    sizes[1] = ((80 - 0) / 10) + 1;

    dimensions[2] = create_range(0, 180, 20); // relative azimuth
    sizes[2] = ((180 - 0) / 20) + 1;

    dimensions[3] = create_range(0.0f, 5.0f, 0.5f); // water vapor
    sizes[3] = ((4.0f - 0.0f) / 0.5f) + 1;

    dimensions[4] = create_range(0.0f, 0.5f, 0.05f); // ozone
    sizes[4] = ((0.5f - 0.0f) / 0.05f) + 1;

    dimensions[5] = new float[17]{0.0f, 0.001f, 0.01f, 0.02f, 0.05f, 0.1f, 0.15f,
                                  0.2f, 0.3f, 0.5f, 0.7f, 1.0f, 1.3f, 1.6f,
                                  2.0f, 3.0f, 5.0f};
    sizes[5] = 17;

    dimensions[6] = new float[3]{750.0f, 1013.0f, 1100.0f}; // pressure at target
    sizes[6] = 3;

    dimensions[7] = new float[3]{-1, -3, -1000}; // sensor altitude
    sizes[7] = 3;

    dimensions[8] = create_range(0.340f, 1.0f, 0.01f); // wavelength
    sizes[8] = ((1.0f - 0.340f) / 0.1f) + 1;

    // TESTING DIMENSION
//    float* dimensions[9];
//    int sizes[9];
//    int indices[9] = {0};
//
//    dimensions[0] = create_range(0, 80, 40); // sun zenith
//    sizes[0] = ((80 - 0) / 40) + 1;
//
//    dimensions[1] = create_range(0, 80, 40); // view zenith
//    sizes[1] = ((80 - 0) / 40) + 1;
//
//    dimensions[2] = create_range(0, 180, 100); // relative azimuth
//    sizes[2] = ((180 - 0) / 100) + 1;
//
//    dimensions[3] = create_range(0.0f, 5.0f, 3.0f); // water vapor
//    sizes[3] = ((4.0f - 0.0f) / 3.0f) + 1;
//
//    dimensions[4] = create_range(0.0f, 0.5f, 0.5f); // ozone
//    sizes[4] = ((0.5f - 0.0f) / 0.5f) + 1;
//
//    dimensions[5] = new float[2]{0.02f, 3.0f};
//    sizes[5] = 2;
//
//    dimensions[6] = new float[1]{1013.0f}; // pressure at target
//    sizes[6] = 1;
//
//    dimensions[7] = new float[1]{-3}; // sensor altitude
//    sizes[7] = 1;
//
//    dimensions[8] = create_range(0.340f, 1.0f, 0.1f); // wavelength
//    sizes[8] = ((1.0f - 0.340f) / 0.01f) + 1;

    // Calculate the number of unique combinations
    int total_combinations = 1;
    for (int size : sizes) {
        total_combinations *= size;
    }

    printf("Number of unique combinations: %d\n", total_combinations);
    std::cout << "Size of float: " << sizeof(float) << " bytes\n";
    std::cout << "Memory size of the array containing atmospheric_radiance_at_sensor: " << (total_combinations * sizeof(float)) / 1073741824.0 << " GB\n";

    float* atmospheric_radiance_at_sensor_values = new float[total_combinations];

    // Get starting time
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i <= total_combinations; ++i) {

        for (int d = 0; d < 8; ++d) {
//            char buffer[20];
//            sprintf(buffer, "%f2. ", dimensions[i][indices[i]]);
            std::ostringstream buffer;
            buffer << std::fixed << std::setprecision(3) << std::setw(8) << std::setfill(' ') << dimensions[d][indices[d]];
            std::string str = buffer.str();
            for (int j = 0; j < str.length(); ++j) {
                params[positions[d] + j] = str[j];
            }
        }

//        cout << "Params set \n";
//        cout << params;
//        cout << "\n";

        // pointer to store the command
        char command[200];

        strcpy(command, "echo \"\n");
        strcat(command, params);
        strcat(command, "\" | ");
        strcat(command, executable);

        std::string jsonOutput = exec(command);

//        std::cout << jsonOutput << std::endl;

        Json::Value root;
        std::string errs;
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();

        if (!reader->parse(jsonOutput.c_str(), jsonOutput.c_str() + jsonOutput.size(), &root, &errs)) {
            std::cout << errs << std::endl;
            return EXIT_FAILURE;
        }

        // Convert the root JSON object to a string
//        std::string rootStr = root.toStyledString();
//        std::cout << "Root: " << rootStr << std::endl;

        // Get the type of the root JSON value
        Json::ValueType type = root.type();
        // Convert the type to a string and print it
        std::string typeStr;
        switch (type) {
            case Json::nullValue: typeStr = "nullValue"; break;
            case Json::intValue: typeStr = "intValue"; break;
            case Json::uintValue: typeStr = "uintValue"; break;
            case Json::realValue: typeStr = "realValue"; break;
            case Json::stringValue: typeStr = "stringValue"; break;
            case Json::booleanValue: typeStr = "booleanValue"; break;
            case Json::arrayValue: typeStr = "arrayValue"; break;
            case Json::objectValue: typeStr = "objectValue"; break;
            default: typeStr = "unknown type";
        }

        // Check if root is a JSON object
        if (root.isObject() && root.isMember("atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]")) {
            // Check if the key exists in the JSON object
            if (root.isMember("atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]")) {

                std::string atmospheric_radiance_at_sensor =
                        root["atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]"].asString();

                atmospheric_radiance_at_sensor_values[i] = std::stof(atmospheric_radiance_at_sensor);

            } else {
                std::cout << "Error: Key 'atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]' does not exist in the JSON object" << std::endl;
            }
        } else {
            std::cout << "Error: root is not a JSON object" << std::endl;
            std::cout << "Type of root: " << typeStr << std::endl;
        }


        // After each iteration, get the current time and calculate the elapsed time
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

        // Compute iterations per second
        if (elapsed > 0) { // To avoid division by zero
            printf("\r(%i/%i) | ", i+1, total_combinations);

            float iterations_per_second = static_cast<float>(i) / elapsed;
            std::cout << "Iterations per second: " << iterations_per_second;

            // Estimate time upon completion
            float estimated_total_time = (total_combinations-i) / iterations_per_second;
            std::cout << " | Estimated time upon completion: " << formatEstimatedTime(estimated_total_time) << std::flush;
        }

        if (i == 10000) {
            break;
        }
    }

    cout << "\n";

    for (int i = 0; i < total_combinations; ++i) {
        std::cout << "\r" << atmospheric_radiance_at_sensor_values[i];
    }

    for (int i = 0; i < 8; ++i) {
        free(dimensions[i]);
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