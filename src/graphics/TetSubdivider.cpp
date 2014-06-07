/*
 * TetSubdivider.cpp
 *
 *  Created on: May 31, 2014
 *      Author: pourya
 */
#include "TetSubdivider.h"
#include "base/Logger.h"

using namespace PS;

namespace PS {
namespace FEM {


TetSubdivider::TetSubdivider(HalfEdgeTetMesh* pMesh) {
	m_lpHEMesh = pMesh;
}

TetSubdivider::~TetSubdivider() {
}

int TetSubdivider::cutEdgesToSplitNode(U32 element, U8 node, double targetDist,
									   U8& cutEdgeCode, U8& cutNodeCode, double (&tEdges)[6]) {

	if(node >= 4)
		return -1;

	cutEdgeCode = 0;
	cutNodeCode = 0;

	int res = 0;
	HalfEdgeTetMesh::ELEM elem = m_lpHEMesh->elemAt(element);
	for(int i=0; i<6; i++) {
		tEdges[i] = 0.0;

		HalfEdgeTetMesh::EDGE edge = m_lpHEMesh->edgeAt( m_lpHEMesh->edge_from_halfedge(elem.halfedge[i]) );

		if(edge.from == node || edge.to == node) {
			cutEdgeCode |= (1 << i);

			if(edge.from == node)
				tEdges[i] = targetDist;
			else
				tEdges[i] = 1.0 - targetDist;

			res ++;
		}
	}

	return res;

}

int TetSubdivider::subdivide(U32 element, U8 cutEdgeCode, U8 cutNodeCode, double tEdges[6]) {
	//Here an element is subdivided to 4 sub elements depending on the codes
	U8 ctCutEdges = 0;

	//6 edges per tet
	for(int i=0; i < 6; i++)
		ctCutEdges += ((cutEdgeCode & (1 << i)) != 0);

	U8 ctCutNodes = 0;

	//4 nodes per tet
	for(int i=0; i < 4; i++)
		ctCutNodes += ((cutNodeCode & (1 << i)) != 0);

	printf("Number of cutEdges: %d, cutNodes: %d \n", ctCutEdges, ctCutNodes);


	//fill the array of virtual nodes
	U32 vnodes[16];
	for(int i=0; i<16; i++)
		vnodes[i] = HalfEdgeTetMesh::INVALID_INDEX;
	const HalfEdgeTetMesh::ELEM& one = m_lpHEMesh->elemAt(element);

	//1st 4 nodes come from the original element
	for(int i=0; i<4; i++)
		vnodes[i] = one.nodes[i];

	//Mask to map edges indices to new generated node indices
	int mapEdgeToMiddleNodes[12] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

	if(ctCutEdges > 0) {

		//loop over edges
		for(int i=0; i < 6; i++) {
			bool isCut = ((cutEdgeCode & (1 << i)) != 0);

			if(isCut) {

				U32 idxNP0, idxNP1;
				U32 idxEdgeToCut = m_lpHEMesh->edge_from_halfedge(one.halfedge[i]);
				if(!m_lpHEMesh->cut_edge(idxEdgeToCut, tEdges[i], &idxNP0, &idxNP1)) {
					LogErrorArg2("Unable to cut edge %d of element %d", i, element);
				}

				//generated nodes
				vnodes[ mapEdgeToMiddleNodes[i*2 + 0] ] = idxNP0;
				vnodes[ mapEdgeToMiddleNodes[i*2 + 1] ] = idxNP1;
			}
		}
	}



	//int edgeMaskPos[6][2] = { {1, 2}, {2, 3}, {3, 1}, {2, 0}, {0, 3}, {0, 1} };
	//int edgeMaskNeg[6][2] = { {3, 2}, {2, 1}, {1, 3}, {3, 0}, {0, 2}, {1, 0} };
	//int faceMaskPos[4][3] = { {1, 2, 3}, {2, 0, 3}, {3, 0, 1}, {1, 0, 2} };
	//int faceMaskNeg[4][3] = { {3, 2, 1}, {3, 0, 2}, {1, 0, 3}, {2, 0, 1} };

	//using the lookup table generate new tet elements and add them to the mesh
	//if 3 cut edges: cases 11, 22, 37, 56
	if(cutEdgeCode == 11) {
		const U32 lut_gentets[4][4] = {{2, 5, 6, 11}, {3, 7, 10, 4}, {3, 1, 4, 11}, {0, 1, 3, 10}};

		//Remove the original element
		m_lpHEMesh->remove_element(element);

		//generate new tets
		for(int e = 0; e < 4; e++) {
			U32 n[4];
			for(int i = 0; i < 4; i++)
				n[i] = vnodes[ lut_gentets[e][i] ];

			m_lpHEMesh->insert_element(n);
		}
	}


	//if 4 cut edges

	return 0;
}


void TetSubdivider::draw() {

	if(m_lpHEMesh)
		m_lpHEMesh->draw();
}

}
}

