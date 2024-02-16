#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace std;

int main() {

    char executable[] = "/home/raphael/CLionProjects/C6S/6sV2.1/sixsV2.1";

//    char params[] = {
//            '0', '\n', //Geometrical conditions igeom
//            // %(sol_zen).2f %(sol_azi).2f %(view_zen).2f %(view_azi).2f 1 1
//            static_cast<char>('30') , '1', '1', '1', '1', '1', '\n',
//            '2', '\n', //Atmospheric conditions, gas (idatm)
//            '0', '\n', // Visibility (km)
//            static_cast<char>('0.02'), '\n', // aot550
//            '0', '\n', //Target altitude as pressure in mb
//            static_cast<char>('-3'), '\n', //Sensor altitude in -km (xpp)
//            // Aircraft water vapor, ozone and aot550
//            static_cast<char>('-1.0'), static_cast<char>('-1.0'), '\n',
//            static_cast<char>('-1.0'), '\n',
//            static_cast<char>('-1'), '\n', // Spectrum monochromatic selection (iwave)
//            static_cast<char>('0.450'), '\n', // wavelength in um
//            '0', '\n', //Ground reflectance (inhomo)
//            '0', '\n',
//            '0', '\n',
//            static_cast<char>('0.002'), '\n',// reflectance
//            static_cast<char>('-1'), '\n', // no atmospheric correction
//            '\0' // null terminator
//    };

    char params[] = "0\n"
    "34.62 229.25 4.41 221.24 1 1\n"
    "2\n"
    "0\n"
    "0.04\n"
    "0\n"
    "-3.0490\n"
    "-1.0 -1.0\n"
    "-1.0\n"
    "-1\n"
    "0.450\n"
    "0\n"
    "0\n"
    "0\n"
    "0.002262\n"
    "-1\n";

//    float params[] = {
//            0, //Geometrical conditions igeom
//            // %(sol_zen).2f %(sol_azi).2f %(view_zen).2f %(view_azi).2f 1 1
//            0 , 0, 0, 0, 1, 1,
//            2, //Atmospheric conditions, gas (idatm)
//            0, // Visibility (km)
//            0.05, // aot550
//            0, //Target altitude as pressure in mb
//            -3.01, //Sensor altitude in -km (xpp)
//            // Aircraft water vapor, ozone and aot550
//            -1.0, -1.0,
//            -1.0,
//            -1, // Spectrum monochromatic selection (iwave)
//            0.450, // wavelength in um
//            0, //Ground reflectance (inhomo)
//            0,
//            0,
//            0.002, // reflectance
//            -1, // no atmospheric correction
//    };

    // pointer to store the command
    char command[200];

    //strcpy(command, executable);  // Copy str1 into result
    strcpy(command, "echo \"\n");   // Add a space to result
    strcat(command, params);

    //Append each float to the command string
    // gives error: *** stack smashing detected ***: terminated
//    for (float param : params) {
//        char float_str[20]; // Assuming each float can be represented in <= 20 characters
//        snprintf(float_str, sizeof(float_str), "%f", param);
//        strcat(command, float_str);
//        strcat(command, " ");
//    }

    strcat(command, "\" | ");// Concatenate str2 to result
    strcat(command, executable);
    strcat(command, " /dev/stdin");


//    cout << command;

    int returnCode = system(command);

    //cout << returnCode;

    // checking if the command was executed successfully
//    if (returnCode == 0) {
//        cout << "Command executed successfully." << returnCode << endl;
//    } else {
//        cout << "Command execution failed or returned "
//                "non-zero: "
//             << returnCode << endl;
//    }

    return (0);
}