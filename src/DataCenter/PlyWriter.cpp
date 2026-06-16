#include "DataCenter/PlyWriter.h"

#include <expected>
#include <fstream>
#include <string>

std::expected<void, std::string> PlyWriter::write(const std::string& outputFile, const std::vector<Triangle>& triangles) const
{
    std::ofstream file(outputFile);
    if (!file) {
        return std::unexpected("Cannot open output file: " + outputFile);
    }

    const auto vertexCount = triangles.size() * 3;

    file << "ply\n";
    file << "format ascii 1.0\n";
    file << "element vertex " << vertexCount << '\n';
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    file << "element face " << triangles.size() << '\n';
    file << "property list uchar uint vertex_indices\n";
    file << "end_header\n";

    for (const Triangle& triangle : triangles) {
        for (const Vec3& vertex : triangle.vertices) {
            file << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
        }
    }

    unsigned int vertexIndex = 0;
    for (const Triangle& triangle : triangles) {
        (void)triangle;
        file << "3 " << vertexIndex << ' ' << vertexIndex + 1 << ' ' << vertexIndex + 2 << '\n';
        vertexIndex += 3;
    }

    if (!file) {
        return std::unexpected("Failed while writing output file: " + outputFile);
    }

    return {};
}
