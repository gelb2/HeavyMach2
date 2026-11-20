// Export the shared decoder's title artwork for the Android launcher build.
#include "Assets/Image.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 1;
    std::vector<std::uint8_t> file{std::istreambuf_iterator<char>(input), {}};
    const auto image = hm2::decodeImage(file);
    if (image.width != 768 || image.height != 1024) return 1;
    std::cout.write(reinterpret_cast<const char*>(image.rgba.data()), image.rgba.size());
    return !std::cout;
}
