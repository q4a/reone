/*
 * Copyright (c) 2020-2021 The reone project contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "gr2file.h"

#include <stdexcept>

#include <boost/algorithm/string.hpp>

#include "../../common/log.h"
#include "../../render/textures.h"

using namespace std;

using namespace reone::resource;

namespace reone {

namespace render {

static constexpr int kMaxMeshNameLength = 64;

Gr2File::Gr2File() : BinaryFile(4, "GAWB") {
}

void Gr2File::doLoad() {
    // Adapted from https://forum.xentax.com/viewtopic.php?f=16&t=9703&start=30#p94880

    seek(0x10);

    uint32_t num50Offsets = readUint32();
    uint32_t gr2Type = readUint32();
    _numMeshes = readUint16();
    uint16_t numTextures = readUint16();
    uint16_t numBones = readUint16();
    uint16_t numAttachs = readUint16();

    seek(0x50);

    uint32_t offset50Offset = readUint32();
    uint32_t offsetMeshHeader = readUint32();
    uint32_t offsetMaterialNameOffsets = readUint32();
    uint32_t offsetBoneStructure = readUint32();
    uint32_t offsetAttachments = readUint32();

    loadMeshHeaders();
    loadMeshNames();
    loadMeshPieces();

    // TODO: load attachments
    // TODO: load materials

    loadMeshes();
    loadModel();
}

void Gr2File::loadMeshHeaders() {
    seek(0x70);

    for (uint16_t i = 0; i < _numMeshes; ++i) {
        MeshHeader header;
        header.nameOffset = readUint32();

        ignore(4);

        header.numPieces = readUint16();
        header.numUsedBones = readUint16();

        ignore(2);

        header.numVertexBytes = readUint16();
        header.numVertices = readUint32();
        header.numFaces = readUint32();
        header.verticesOffset = readUint32();
        header.piecesOffset = readUint32();
        header.facesOffset = readUint32();
        header.bonesOffset = readUint32();

        _meshHeaders.push_back(move(header));
    }
}

void Gr2File::loadMeshNames() {
    for (uint16_t i = 0; i < _numMeshes; ++i) {
        seek(_meshHeaders[i].nameOffset);
        _meshNames.push_back(readCString(kMaxMeshNameLength));
    }
}

void Gr2File::loadMeshPieces() {
    for (uint16_t i = 0; i < _numMeshes; ++i) {
        vector<MeshPiece> pieces;
        pieces.reserve(_meshHeaders[i].numPieces);

        seek(_meshHeaders[i].piecesOffset);
        for (uint16_t j = 0; j < _meshHeaders[i].numPieces; ++j) {
            MeshPiece piece;
            piece.materialFacesIdx = readUint32();
            piece.numMaterialFaces = readUint32();
            piece.textureId = readUint32();

            ignore(24);

            pieces.push_back(move(piece));
        }

        _meshPieces.push_back(move(pieces));
    }
}

static float convertHalfFloatToFloat(uint16_t value) {
    uint32_t sign = (value & 0x8000) ? 1 : 0;
    uint32_t exponent = ((value & 0x7c00) >> 10) - 16;
    uint32_t mantissa = value & 0x03ff;
    uint32_t tmp = ((sign << 31) | ((exponent + 127) << 23) | (mantissa << 13));
    float result = 2.0f * *reinterpret_cast<float *>(&tmp);
    return result;
}

void Gr2File::loadMeshes() {
    for (uint16_t i = 0; i < _numMeshes; ++i) {
        if (boost::contains(_meshNames[i], "collision")) continue;

        vector<float> vertices;
        seek(_meshHeaders[i].verticesOffset);
        vector<uint8_t> gr2Vertices(readArray<uint8_t>(static_cast<size_t>(_meshHeaders[i].numVertices) * _meshHeaders[i].numVertexBytes));
        const uint8_t *gr2VerticesPtr = &gr2Vertices[0];
        for (uint32_t j = 0; j < _meshHeaders[i].numVertices; ++j) {
            vertices.push_back(*reinterpret_cast<const float *>(gr2VerticesPtr + 0));
            vertices.push_back(*reinterpret_cast<const float *>(gr2VerticesPtr + 4));
            vertices.push_back(*reinterpret_cast<const float *>(gr2VerticesPtr + 8));

            // Leave space for normals
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);

            switch (_meshHeaders[i].numVertexBytes) {
                case 12:
                    break;
                case 24: {
                    vertices.push_back(convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 20)));
                    vertices.push_back(1.0f - convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 22)));
                    break;
                }
                case 32:
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 12)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 13)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 14)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 15)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 16)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 17)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 18)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 19)));
                    vertices.push_back(convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 28)));
                    vertices.push_back(1.0f - convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 30)));
                    break;
                case 36:
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 12)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 13)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 14)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 15)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 16)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 17)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 18)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 19)));
                    vertices.push_back(convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 28)));
                    vertices.push_back(1.0f - convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 32)));
                    break;
                case 40:
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 12)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 13)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 14)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 15)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 16)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 17)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 18)));
                    vertices.push_back(static_cast<float>(*reinterpret_cast<const uint8_t *>(gr2VerticesPtr + 19)));
                    vertices.push_back(convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 32)));
                    vertices.push_back(1.0f - convertHalfFloatToFloat(*reinterpret_cast<const uint16_t *>(gr2VerticesPtr + 34)));
                    break;
                default:
                    break;
            }
            gr2VerticesPtr += _meshHeaders[i].numVertexBytes;
        }

        Mesh::VertexOffsets offsets;
        offsets.normals = 3 * sizeof(float);
        switch (_meshHeaders[i].numVertexBytes) {
            case 12:
                offsets.stride = 6 * sizeof(float);
                break;
            case 24:
                offsets.texCoords1 = 6 * sizeof(float);
                offsets.stride = 8 * sizeof(float);
                break;
            case 32:
            case 36:
            case 40:
                offsets.boneWeights = 6 * sizeof(float);
                offsets.boneIndices = 10 * sizeof(float);
                offsets.texCoords1 = 14 * sizeof(float);
                offsets.stride = 16 * sizeof(float);
                break;
            default:
                throw new runtime_error("Unsupported GR2 vertex size: " + to_string(_meshHeaders[i].numVertexBytes));
        }

        vector<uint16_t> indices;
        seek(_meshHeaders[i].facesOffset);
        for (uint16_t j = 0; j < _meshHeaders[i].numPieces; ++j) {
            vector<uint16_t> pieceIndices(readArray<uint16_t>(3 * _meshPieces[i][j].numMaterialFaces));
            indices.insert(indices.end(), pieceIndices.begin(), pieceIndices.end());
        }

        auto mesh = make_shared<ModelMesh>(true, 0, true);
        mesh->_vertexCount = _meshHeaders[i].numVertices;
        mesh->_vertices = move(vertices);
        mesh->_offsets = move(offsets);
        mesh->_indices = move(indices);
        mesh->_diffuseColor = glm::vec3(0.2f);
        mesh->_ambientColor = glm::vec3(0.8f);
        mesh->_diffuse = Textures::instance().get("acklay", TextureType::Diffuse);
        mesh->computeNormals();
        mesh->computeAABB();
        _meshes.push_back(move(mesh));
    }
}

void Gr2File::loadModel() {
    glm::mat4 transform(1.0f);
    transform *= glm::mat4_cast(glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0, 0.0f)));
    transform = glm::scale(transform, glm::vec3(10.0f));

    int index = 0;
    auto rootNode = make_shared<ModelNode>(index++);
    rootNode->_localTransform = transform;
    rootNode->_absTransform = rootNode->_localTransform;
    rootNode->_absTransformInv = glm::inverse(rootNode->_absTransform);

    for (uint16_t i = 0; i < _numMeshes; ++i) {
        auto node = make_shared<ModelNode>(index);
        node->_nodeNumber = index;
        node->_name = _meshNames[i];
        node->_mesh = _meshes[i];
        node->_absTransform = rootNode->_absTransform;
        node->_absTransformInv = glm::inverse(node->_absTransform);
        rootNode->_children.push_back(move(node));

        ++index;
    }

    _model = make_shared<Model>("", rootNode, vector<unique_ptr<Animation>>());
    _model->_classification = Model::Classification::Character; // prevent shading
    _model->initGL();
}

} // namespace render

} // namespace reone
