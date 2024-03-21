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
#include <atomic>

using namespace std;

std::atomic<int> counter(0);

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
        // If we've processed all dimensions, add the current combination to the result
        result.push_back(temp);
    } else {
        // Otherwise, for each value in the current dimension, add it to the current combination and recurse on the next dimension
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

// This is the function that will be run by each thread
void runModelAndAccumulate(const char* executable, int start, int end, std::vector<std::vector<float>> combination, float* atmospheric_radiance_at_sensor_values) {
    for (int i = start; i < end; ++i) {

        char params[800];
        sprintf(params,
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
                "-1 # IRAPP no atmospheric correction\n",
                combination[i][0],
                combination[i][1],
                combination[i][2],
                combination[i][3],
                combination[i][4],
                combination[i][5],
                combination[i][6]
        );

        // pointer to store the command
        char command[200];

        strcpy(command, "echo \"\n");
        strcat(command, params);
        strcat(command, "\" | ");
        strcat(command, executable);

//        std::cout << command << std::endl;

//        std::string jsonOutput = exec(command);
//        std::cout << jsonOutput << std::endl;
        auto result = exec(command);
        std::string jsonOutput = result.first;
        int exitCode = result.second;

        //cout << exitCode << "\n";
        if (exitCode != 0) {
            printf("Error on iteration: %i\n", start + i);
            cout << counter << std::endl;
            std::cout << command << std::endl;
            throw std::runtime_error("Command failed with exit code: " + std::to_string(exitCode));
        }

//        std::cout << jsonOutput << std::endl;

        Json::Value root;
        std::string errs;
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();

        if (!reader->parse(jsonOutput.c_str(), jsonOutput.c_str() + jsonOutput.size(), &root, &errs)) {
            std::cout << errs << std::endl;
            //return EXIT_FAILURE;
        }

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

                float atmospheric_radiance_at_sensor =
                        root["atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]"].asFloat();

                atmospheric_radiance_at_sensor_values[i] = atmospheric_radiance_at_sensor;

            } else {
                std::cout << "Error: Key 'atmospheric_radiance_at_sensor_[W m-2 sr-1 um-1]' does not exist in the JSON object" << std::endl;
            }
        } else {
            throw std::runtime_error("Error: root is not a JSON object: " + typeStr);
        }

        counter++;
    }
}

#define ERRCODE 2
#define ERR(e) {printf("Error: %s\n", nc_strerror(e)); exit(ERRCODE);}

int main() {

    char executable[] = "/home/raphael/CLionProjects/C6S/6sV2.1/sixsV2.1";

    // GOOD DIMENSION
//    std::vector<std::vector<float>> dimensions = {
//            create_range(0, 80, 10), // sun zenith
//            create_range(0, 80, 10), // view zenith
//            create_range(0, 180, 20), // relative azimuth
//            {0.0f, 0.001f, 0.01f, 0.02f, 0.05f, 0.1f, 0.15f,
//             0.2f, 0.3f, 0.5f, 0.7f, 1.0f, 1.3f, 1.6f,
//             2.0f, 3.0f, 5.0f}, // taer550
//            {750.0f, 1013.0f, 1100.0f}, // pressure at target
//            {-1, -3}, // sensor altitude
//            create_range(0.340f, 1.0f, 0.01f) // wavelength
//    };

    // TEST DIMENSION
    std::vector<std::vector<float>> dimensions = {
            create_range(0, 80, 40), // sun zenith
            create_range(0, 80, 40), // view zenith
            create_range(0, 180, 30), // relative azimuth
            {0.0f, 3.0f}, // taer550
            {1013.0f}, // pressure at target
            {-1}, // sensor altitude
            create_range(0.340f, 1.0f, 0.1f) // wavelength
    };

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

    float* atmospheric_radiance_at_sensor_values = new float[combination.size()];

    // Determine the number of threads to use
    int numThreads = std::thread::hardware_concurrency() -11;

    // Create a vector to hold the thread objects
    std::vector<std::thread> threads(numThreads);

    printf("running on %i threads \n", numThreads);

    // Calculate the number of iterations per thread
    int iterationsPerThread = combination.size() / numThreads;

    // Start each thread
    for (int t = 0; t < numThreads; ++t) {
        // Calculate the start and end indices for this thread
        int start = t * iterationsPerThread;
        int end = (t == numThreads - 1) ? combination.size() : start + iterationsPerThread;

        // Start the thread
        threads[t] = std::thread(runModelAndAccumulate, executable, start, end, combination, atmospheric_radiance_at_sensor_values);
    }

    auto start = std::chrono::high_resolution_clock::now();

//    while (counter < combination.size()) {
//        //std::this_thread::sleep_for(std::chrono::seconds(1)); // Sleep for a second
//
//        auto now = std::chrono::high_resolution_clock::now();
//        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
//
//        if (elapsed > 0) { // To avoid division by zero
//
//            printf("\r(%i/%i) | ", static_cast<int>(counter), combination.size());
//
//            float iterations_per_second = static_cast<float>(counter) / elapsed;
//            std::cout << "Iterations per second: " << iterations_per_second;
//
//            float estimated_total_time = (combination.size() - counter) / iterations_per_second;
//            std::cout << " | Estimated time upon completion: " << formatEstimatedTime(estimated_total_time) << std::flush;
//        }
//    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

//    for (int i = 0; i < combination.size(); ++i) {
//        std::cout << "\n" << atmospheric_radiance_at_sensor_values[i];
//    }

    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now- start).count();
    printf("Elapsed seconds: %i", elapsed);

    return (0);
}