#include "toojpeg.h"
#include <stdio.h>
#include <cstdint>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <limits>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

// The TooJPEG library, which I've decided to use needs a function to write bytes to the output.
// The signature of this function must be void (*)(unsigned char) so I cannot pass the file pointer as an argument.
// But the function needs to see the file pointer somehow, so it needs to be global.
FILE* newjpeg = nullptr;
void writeToOutput(unsigned char byte) {
    if (newjpeg != nullptr) {
        fwrite(&byte, sizeof(byte), 1, newjpeg);
    }
    else {
        throw "The output file needs to be initialized.";
    }
}

// Memcpy is a dangerous function.
// We should make sure about not to copy to a null pointer and from a null pointer.
// Also we need to check the number of the copied bytes is not more than the size of the destination buffer.
// In that case a bufferoverflow would happen and I would like to prevent it.
// I know that there is a safer memcpy called memcpy_s but its not a part of the original C11 standard,
// so in that case we would lose portability.
// That's why I've decided to implement a my own wrapper around memcpy, that makes sure about these things.

void s_memcpy(void* to, size_t tos, const void* from, size_t numofbytes) {
    if (to == nullptr || from == nullptr) {
        throw "Invalid poiners!";
    }
    if (tos < numofbytes) {
        throw "Buffer overflow!";
    }
    memcpy(to, from, numofbytes);
}

// Also fread is not secure.
// I pass the size of the target memory as tos, the size of each obj as s, and the number of them as c
size_t s_fread(void* to, size_t tos, size_t s, size_t c, FILE* fptr) {
    if (to == nullptr || fptr == nullptr) {
        throw "Invalid pointers!";
    }
    size_t req_size = s*c;
    if (tos < req_size) {
        throw "Overflow!";
    }
    size_t res = fread(to, s, c, fptr);
    return res;
}

void check_unint64_overflow (uint64_t a, uint64_t b) {
    if (b > numeric_limits<uint64_t>::max() - a) {
        throw "uint_64 overflow!";
    }
}

// Reading in CIFF formatted data.
uint64_t beolvas_ciff(FILE* file, const char* filename) {
    // Start to count the bytes which were read
    uint64_t bytesread = 0;
    // 4 bytes for the magic and \0
    char magicbuff[5];
    magicbuff[4] = '\0';
    // Read the first 4 bytes from the header
    try {
        uint64_t res = s_fread(magicbuff, sizeof(magicbuff), 1, 4, file);
        check_unint64_overflow(res, bytesread);
        bytesread += res;
    } catch (const char* msg) {
        cout << msg << endl;
        throw "Error reading in CIFF magic bits!";
    }
    string magic(magicbuff);
    cout << magic << endl;
    if (magic.compare("CIFF") != 0) {
        throw "Wrong magic bits!";
    }
    // Most of the numbers in the header are 8B long unsigned integers.
    char buff[8];
    uint64_t headersize, contentsize, width, height;
    try {
        // Read 8 bytes for headersize
        bytesread += s_fread(buff, sizeof(buff), 1, 8, file);
        s_memcpy(&headersize, sizeof(headersize), buff, sizeof(buff));
        // Read 8 bytes for contentsize
        bytesread += s_fread(buff, sizeof(buff), 1, 8, file);
        s_memcpy(&contentsize, sizeof(contentsize), buff, sizeof(buff));
        // Read 8 bytes of width and height
        bytesread += s_fread(buff, sizeof(buff), 1, 8, file);
        s_memcpy(&width, sizeof(width), buff, sizeof(buff));
        bytesread += s_fread(buff, sizeof(buff), 1, 8, file);
        s_memcpy(&height, sizeof(height), buff, sizeof(buff));
    } catch (const char* msg) {
        cout << msg << endl;
        throw "Error reading in CIFF numbers!";
    }
    // Check if contentsize equals width*height*3
    if(contentsize != width*height*3) {
        throw "Wrong content size!";
    }
    // Start reading the caption byte-by-byte
    vector<char> captionbuff;
    char bytebuff;
    while (bytebuff != '\n') {
        try {
            // Variable length caption should check the number of the bytes before adding.
            uint64_t res = s_fread(&bytebuff, sizeof(bytebuff), 1, 1, file);
            check_unint64_overflow(res, bytesread);
            bytesread += res;
        } catch (const char* msg) {
            cout << msg << endl;
            throw "Error reading in CIFF caption!";
        }
        if(bytebuff != '\n') {
            captionbuff.push_back(bytebuff);
        }
    }
    string caption(captionbuff.begin(), captionbuff.end());
    cout << caption << endl;
    // Start reading the tags byte-by-byte each of them is terminated by \0 and cannot be multiline
    vector<string> tags;
    while (bytesread < headersize) {
        vector<char> tag;
        while (bytebuff != '\0') {
            try {
                // Variable length caption should check the number of the bytes before adding.
                uint64_t res = s_fread(&bytebuff, sizeof(bytebuff), 1, 1, file);
                check_unint64_overflow(res, bytesread);
                bytesread += res;
            } catch (const char* msg) {
                cout << msg << endl;
                throw "Error reading in CIFF tags!";
            }
            if (bytebuff == '\n') {
                throw "Tags cannot be multiline!";
            }
            if (bytebuff != '\0') {
                tag.push_back(bytebuff);
            }
        }
        cout << "Tag:" << endl;
        string stag(tag.begin(), tag.end());
        cout << stag << endl;
        tags.push_back(stag);
        // Reset bytebuff
        bytebuff = 'a';
    }
    // Here we have finished reading in CIFF header,
    // We need to validate the number of the bytes against the header length
    if (headersize != bytesread) {
        throw "The defined size of the CIFF header and the read in bytes doesn't match!";
    }
    // From here you can read the ciff content
    // Each pixel byte-by-byte
    unsigned char pixels[contentsize];
    uint64_t i = 0;
    while (i < contentsize) {
        // Variable length caption should check the number of the bytes before adding.
        uint64_t res = s_fread(pixels+i, sizeof(unsigned char), 1, 1, file);
        check_unint64_overflow(res, bytesread);
        bytesread += res;
        i++;
    }
    string strfilename = filename;
    string strfile = strfilename.substr(0, (strfilename.length()-5));
    strfile.append(".jpg");
    cout << strfile.c_str() << endl;
    newjpeg = fopen(strfile.c_str(), "wb");
    if (newjpeg == NULL) {
        fclose(newjpeg);
        throw "Error opening the destination JPG file!";
    }
    bool done = TooJpeg::writeJpeg(writeToOutput, pixels, width, height, true, 90, false, "created from CIFF content");
    if (done != true) {
        fclose(newjpeg);
        throw "Error during JPG creation!";
    }
    fclose(newjpeg);
    fclose(file);
    return bytesread;
}

// Reading in CAFF header, receives file pointer and the length of the block.
uint64_t read_caff_header(FILE* fptr, uint64_t blen) {
    // Start to count bytes in the header which were successfully read in
    uint64_t actuallen = 0;
    char magicbuff[5];
    try {
        actuallen += s_fread(magicbuff, sizeof(magicbuff), 1, 4, fptr);
        magicbuff[4] = '\0';
    } catch (const char* msg) {
        cout << msg << endl;
        throw "Error reading in CAFF header magic bits!";
    }
    string magic(magicbuff);
    if(magic.compare("CAFF") != 0) {
        throw "Wrong magic bits!";
    }
    cout << magic << endl;
    char headerbuff[8];
    uint64_t headerlen, animcount;
    try {
        actuallen += s_fread(headerbuff, sizeof(headerbuff), 1, 8, fptr);
        s_memcpy(&headerlen, sizeof(headerlen), headerbuff, sizeof(headerbuff));
        actuallen += s_fread(headerbuff, sizeof(headerbuff), 1, 8, fptr);
        s_memcpy(&animcount, sizeof(animcount), headerbuff, sizeof(headerbuff));
    } catch (const char* msg) {
        cout << msg << endl;
        throw "Error converting CAFF header numbers!";
    }
    cout << "headerlen:" << endl;
    cout << headerlen << endl;
    cout << "animcount:" << endl;
    cout << animcount << endl;
    // Validating length
    // The number of bytes which were read in should equal to the received block length
    // and the header length
    // and the outer block length should be also equal to the length of this header.
    if (actuallen != blen || actuallen != headerlen || blen != headerlen) {
        throw "The block length is not equal to the number of bytes read in.";
    }
    return animcount;
}

// Reading in CAFF credits header. Receives a file pointer and the length of the header.
void read_caff_credits(FILE* fptr, uint64_t blen) {
    // Start to count bytes in the header which were successfully read in
    uint64_t actuallen = 0;
    char yearbuff[2];
    char monthbuff, daybuff, hourbuff, minutebuff;
    uint16_t year;
    uint8_t mon, day, h, minute;
    try {
        actuallen += s_fread(yearbuff, sizeof(yearbuff), 1, 2, fptr);
        actuallen += s_fread(&monthbuff, sizeof(monthbuff), 1, 1, fptr);
        actuallen += s_fread(&hourbuff, sizeof(hourbuff), 1, 1, fptr);
        actuallen += s_fread(&daybuff, sizeof(daybuff), 1, 1, fptr);
        actuallen += s_fread(&minutebuff, sizeof(minutebuff), 1, 1, fptr);
        s_memcpy(&year, sizeof(year), yearbuff, sizeof(yearbuff));
        s_memcpy(&mon, sizeof(mon), &monthbuff, sizeof(monthbuff));
        s_memcpy(&day, sizeof(day), &daybuff, sizeof(daybuff));
        s_memcpy(&h, sizeof(h), &hourbuff, sizeof(hourbuff));
        s_memcpy(&minute, sizeof(minute), &minutebuff, sizeof(minutebuff));
    } catch (const char* msg){
        cout << msg << endl;
        throw "Error reading in CAFF credit header!";
    }
    // Validate the date:
    if(year < 0) {
        throw "Year is not valid!";
    }
    if (mon > 12 || mon < 0) {
        throw "Month is not valid!";
    }
    if (day > 31 || day < 0) {
        throw "Day is not valid!";
    }
    if (h > 23 || h < 0) {
        throw "Hour is not valid!";
    }
    if (minute > 59 || minute < 0) {
        throw "Minute is not valid!";
    }
    // 8B long name length buffer
    char namelenbuff[8];
    // 8B long unsigned integer specifying the creators name length.
    uint64_t namelen;
    try {
        actuallen += s_fread(namelenbuff, sizeof(namelenbuff), 1, 8, fptr);
        s_memcpy(&namelen, sizeof(namelen), namelenbuff, sizeof(namelenbuff));
    } catch (const char* msg) {
        cout << msg << endl;
        throw "Error converting creator names length in the CAFF credits!";
    }
    cout << "Creator name len: " << endl;
    cout << namelen << endl;
    char name[namelen+1];
    uint64_t actualnamelen = 0;
    actualnamelen = s_fread(name, sizeof(name), 1, namelen, fptr);
    actuallen += actualnamelen;
    name[namelen] = '\0';
    cout << name << endl;
    // Validate lengths read in
    // The name length should equal to the specified value
    if (actualnamelen != namelen || actuallen != blen) {
        throw "CAFF credits header doesn't match!";
    }
}

// Reading in CAFF anim header. Receives a file pointer, the assumed length of the header,
// the number of the animations and the filename
void read_caff_anim(FILE* fptr, uint64_t blen, uint64_t animcount, const char* filename) {
    // Start to count bytes in the header which were successfully read in
    uint64_t actuallen = 0;
    char durationbuff[8];
    uint64_t durationms;
    try {
        actuallen += s_fread(durationbuff, sizeof(durationbuff), 1, 8, fptr);
        s_memcpy(&durationms, sizeof(durationms), durationbuff, sizeof(durationbuff));
    } catch (const char* msg) {
        cout << msg << endl;
        throw "Error reading in CAFF animation length!";
    }
    cout << durationms << endl;
    // If we are done with the duration, then we can move on to the actual CIFF content
    actuallen += beolvas_ciff(fptr, filename);
    if (blen != actuallen) {
        throw "CAFF animation length doesn't match with the block size!";
    }
}

void beolvas_caff(const char* filename) {
    // Try to open the file
    cout << filename << endl;
    FILE* fptr = fopen(filename, "rb");
    if (fptr == NULL) {
        throw "Error opening the caff file.";
    }
    // 1B unsigned integer for the ID
    uint8_t id;
    char idbuff;
    // Number of animations in CAFF, this will be gathered from the first header
    uint64_t numofanims;
    // Flag to show if header was the first.
    bool headerfirst = false;
    // In case of CAFF files its enough to convert only the first CIFF picture
    bool firstciff = false;
    // Reading in the ID
    // Every CAFF block starts with the ID (1B number)
    while(s_fread(&idbuff, sizeof(idbuff), 1, 1, fptr) == 1 && firstciff != true) {
        try {
            s_memcpy(&id, sizeof(id),  &idbuff, sizeof(idbuff));
        } catch (const char* msg) {
            fclose(fptr);
            cout << msg << endl;
            throw "Error converting ID!";
        }
        if(id > 3 || id < 1) {
            fclose(fptr);
            throw "Error converting ID!";
        }
        // Reading in the block length
        uint64_t blocklen;
        char lenbuff[8];
        fread(lenbuff, 1, 8, fptr);
        try {
            s_memcpy(&blocklen, sizeof(blocklen), lenbuff, sizeof(lenbuff));
        } catch (const char* msg) {
            fclose(fptr);
            cout << msg << endl;
            throw "Error converting length!";
        }
        // Switch to the ID to decide the header type
        switch(id) {
            case 1:
                try {
                    numofanims = read_caff_header(fptr, blocklen);
                } catch (const char * msg) {
                    fclose(fptr);
                    cout << msg << endl;
                    throw "Error reading CAFF header!";
                }
                headerfirst = true;
                break;
            case 2:
                if (headerfirst == false) {
                    fclose(fptr);
                    throw "CAFF header should be the first block!";
                }
                try {
                    read_caff_credits(fptr, blocklen);
                } catch (const char * msg) {
                    fclose(fptr);
                    cout << msg << endl;
                    throw "Error reading CAFF credits!";
                }
                break;
            case 3:
                if (headerfirst == false) {
                    fclose(fptr);
                    throw "CAFF header should be the first block!";
                }
                try {
                    read_caff_anim(fptr, blocklen,  numofanims, filename);
                    firstciff = true;
                } catch (const char * msg) {
                    fclose(fptr);
                    cout << msg << endl;
                    throw "Error reading CAFF animation!";
                }
                break;
            default:
                fclose(fptr);
                throw "Invalid type!"; //Throw an exception if type is not defined
                break;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        cout << "Wrong number of args." << endl;
        return -1;
    }
    // Prevent too long filenames or any other arguments
    if (strlen(argv[2]) > FILENAME_MAX || strlen(argv[1]) != 5) {
        cout << "Wrong argument lengths!" << endl;
        return -1;
    }
    // Here we validate the prompt with the extension of .ciff
    if(strcmp(argv[1], "-ciff") == 0) {
        string path = argv[2];
        //check if the extension equals to the required if not return -1
        if (path.substr((path.length()-4), 4).compare("ciff") == 0) {
            FILE* fptr = fopen(path.c_str(), "rb");
            if (fptr == NULL) {
                cout << "Error opening the CIFF file!" << endl;
            }
            try {
                beolvas_ciff(fptr, path.c_str());
            } catch (const char* msg) {
                fclose(fptr);
                cout << msg << endl;
                return -1;
            }
        }
        else {
            cout << "-ciff parsing requires a .ciff format file." << endl;
            return -1;
        }
    }
    // Here we validate the prompt with the extension of .caff
    else if(strcmp(argv[1], "-caff") == 0) {
        string path = argv[2];
        cout << path.substr((path.length()-4), 4) << endl;
        if (path.substr((path.length()-4), 4).compare("caff") == 0) {
            try {
                beolvas_caff(path.c_str());
            } catch (const char* msg) {
                cout << msg << endl;
                return -1;
            }
        }
        else {
            cout << "-caff parsing requires a .caff format file." << endl;
            return -1;
        }
    }
    else {
        // If the first arg is not -ciff or -caff return -1
        cout << "The first arg needs to be -ciff or -caff!" << endl;
        return -1;
    }
    return 0;
}
