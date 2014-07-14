/*
 * TetMesh.h
 *
 *  Created on: May 6, 2014
 *      Author: pourya
 */

#ifndef WINGEDEDGETETMESH_H_
#define WINGEDEDGETETMESH_H_

#include "graphics/SGNode.h"
#include <functional>

/*!Stories:
 * 1. Iterate over edges
 * 2. Setup by adding vertices and tet elements
 * 3. Store rest positions for all vertices
 * 4. Add vertices and split edges using new vertices
 * 5. Iterate over elements
 *
*/
using namespace PS;
using namespace PS::SG;
using namespace std;

#define FACE_SHIFT_A 0
#define FACE_SHIFT_B 21
#define FACE_SHIFT_C 42

#define HEDGE_SHIFT_A 0
#define HEDGE_SHIFT_B 32


//can keep up to 2097151 nodes
#define FACE_BITMASK 0x001FFFFF
#define HEDGE_BITMASK 0xFFFFFFFF

#define FACEID_HASHSIZE (U64)(1<<(3*FACE_SHIFT_B))
#define FACEID_FROM_IDX(a,b,c) (((U64) ((c) & FACE_BITMASK) << FACE_SHIFT_C) | ((U64) ((b) & FACE_BITMASK) << FACE_SHIFT_B) | ((a) & FACE_BITMASK))

#define HEDGEID_HASHSIZE	(U64)(1<<(HEDGE_SHIFT_B))
#define HEDGEID_FROM_IDX(a,b)	(((U64)((b) & HEDGE_BITMASK) << HEDGE_SHIFT_B) | ((a) & HEDGE_BITMASK))

namespace PS {
namespace FEM {

//To track changes made to the topology of the mesh the following events are generated:
// 1. Node added/Removed
// 2. Edge added/Removed
// 3. Face added/Removed
// 4. Element added/Removed



//template <typename T>
class HalfEdgeTetMesh : public SGNode {
public:
	static const U32 INVALID_INDEX = -1;

	enum ErrorCodes {
		err_op_failed = -1,
		err_elem_not_found = -2,
		err_face_not_found = -3,
		err_edge_not_found = -4,
		err_node_not_found = -5,
	};

	//vertices
	struct NODE {
		vec3d pos;
		vec3d restpos;
		U32 outHE;

		NODE& operator = (const NODE& A) {
			pos = A.pos;
			restpos = A.restpos;
			outHE = A.outHE;
			return (*this);
		}
	};

	//half-edge
	class HEDGE{
	public:
		//Vertex
		U32 from, to;

		//Face
		U32 face;

		//HEDGES
		U32 prev, next, opposite;

		//number of references
		U8 refs;


		HEDGE() { init();}
		HEDGE(U32 from_, U32 to_) {
			init();
			from = from_;
			to = to_;
		};

		void init() {
			from = to = face =  INVALID_INDEX;
			prev = next = opposite = INVALID_INDEX;
			refs = 0;
		}

		HEDGE& operator = (const HEDGE& A) {

			from = A.from;
			to = A.to;
			face = A.face;
			prev = A.prev;
			next = A.next;
			opposite = A.opposite;
			refs = A.refs;

			return(*this);
		};
	};

	//Key to access half-edges in a unique order
	struct HEdgeKey
	{
		HEdgeKey(U64 k) { this->key = k; }
		HEdgeKey(U32 a, U32 b) {
	    	key = HEDGEID_FROM_IDX(a, b);
	    }

	    bool operator<(const HEdgeKey& k) const { return key < k.key; }

	    bool operator>(const HEdgeKey& k) const { return key > k.key; }

	    void operator=(const HEdgeKey& other) {
	    	this->key = other.key;
	    }

	    U64 key;
	};


	//edge
	class EDGE {
	public:
		U32 from, to;
		U32 lface, rface;

		EDGE() { init();}
		EDGE(const HEDGE& e1, const HEDGE& e2) {
			setup(e1, e2);
		}

		void init() {
			from = to = lface = rface = INVALID_INDEX;
		}

		void setup(const HEDGE& e1, const HEDGE& e2) {
			assert(e1.from == e2.to && e1.to == e2.from);
			from = e1.from;
			to = e1.to;
			lface = e1.face;
			rface = e2.face;
		}

		EDGE& operator =(const EDGE& A) {
			from = A.from;
			to = A.to;
			lface = A.lface;
			rface = A.rface;

			return (*this);
		}
	};

	//face
	class FACE {
	public:
		U32 halfedge[3];
		U8 refs;
		bool removed;

		FACE() {
			init();
		}

		void init() {
			for(int i=0; i<3; i++)
				halfedge[i] = INVALID_INDEX;
			refs = 0;
			removed = false;
		}

		FACE& operator = (const FACE& A) {
			for(int i=0; i<3; i++)
				halfedge[i] = A.halfedge[i];
			refs = A.refs;
			removed = A.removed;
			return (*this);
		}
	};

	//Key to access faces in a unique order
	struct FaceKey
	{
		FaceKey(U64 k) { this->key = k; }
	    FaceKey(U32 a, U32 b, U32 c) {
	    	order_lo2hi(a, b, c);
	    	key = FACEID_FROM_IDX(a, b, c);
	    }

	    static void order_lo2hi(U32& a, U32& b, U32& c) {
	    	if(a > b)
	    		swap(a, b);
	    	if(b > c)
	    		swap(b, c);
	    	if(a > b)
	    		swap(a, b);
	    }

	    bool operator<(const FaceKey& k) const { return key < k.key; }

	    bool operator>(const FaceKey& k) const { return key > k.key; }

	    void operator=(const FaceKey& other) {
	    	this->key = other.key;
	    }


	    U64 key;
	};


	//elements
	class ELEM {
	public:
		U32 faces[4];
		U32 nodes[4];
		U32 halfedge[6];
		bool posDet;
		bool removed;

		ELEM() {
			init();
		}

		void init() {
			for(int i=0; i<4; i++) {
				faces[i] = INVALID_INDEX;
				nodes[i] = INVALID_INDEX;
			}
			for(int i=0; i<6; i++)
				halfedge[i] = INVALID_INDEX;
			posDet = false;
			removed = false;
		}

		ELEM& operator = (const ELEM& A) {
			for(int i=0; i<4; i++) {
				faces[i] = A.faces[i];
				nodes[i] = A.nodes[i];
			}
			for(int i=0; i<6; i++)
				halfedge[i] = A.halfedge[i];
			posDet = A.posDet;
			removed = A.removed;
			return (*this);
		}
	};



	enum TopologyEvent {teAdded, teRemoved};
	typedef std::function<void(HalfEdgeTetMesh::NODE, U32 handle, TopologyEvent event)> OnNodeEvent;
	typedef std::function<void(HalfEdgeTetMesh::EDGE, U32 handle, TopologyEvent event)> OnEdgeEvent;
	typedef std::function<void(HalfEdgeTetMesh::FACE, U32 handle, TopologyEvent event)> OnFaceEvent;
	typedef std::function<void(HalfEdgeTetMesh::ELEM, U32 handle, TopologyEvent event)> OnElementEvent;
public:
	HalfEdgeTetMesh();
	HalfEdgeTetMesh(const HalfEdgeTetMesh& other);
	HalfEdgeTetMesh(U32 ctVertices, double* vertices, U32 ctElements, U32* elements);
	HalfEdgeTetMesh(const vector<double>& vertices, const vector<U32>& elements);
	virtual ~HalfEdgeTetMesh();

	//Create one tet
	static HalfEdgeTetMesh* CreateOneTet();

	//Topology events
	void setOnNodeEventCallback(OnNodeEvent f);
	void setOnEdgeEventCallback(OnEdgeEvent f);
	void setOnFaceEventCallback(OnFaceEvent f);
	void setOnElemEventCallback(OnElementEvent f);

	//Build
	bool setup(const vector<double>& vertices, const vector<U32>& elements);
	bool setup(U32 ctVertices, const double* vertices, U32 ctElements, const U32* elements);
	void cleanup();
	void printInfo() const;

	double computeDeterminant(U32 idxNodes[4]) const;
	static double ComputeElementDeterminant(const vec3d v[4]);

	//Index control
	inline bool isElemIndex(U32 i) const { return (i < m_vElements.size());}
	inline bool isFaceIndex(U32 i) const { return (i < m_vFaces.size());}
	inline bool isHalfEdgeIndex(U32 i) const { return (i < m_vHalfEdges.size());}
	inline bool isEdgeIndex(U32 i) const { return (i < countEdges());}
	inline bool isNodeIndex(U32 i) const { return (i < m_vNodes.size());}

	//access
	bool getFaceNodes(U32 i, U32* nodes[3]) const;

	ELEM& elemAt(U32 i);
	FACE& faceAt(U32 i);
	HEDGE& halfedgeAt(U32 i);
	NODE& nodeAt(U32 i);
	EDGE edgeAt(U32 i) const;

	const ELEM& const_elemAt(U32 i) const;
	const FACE& const_faceAt(U32 i) const;
	const HEDGE& const_halfedgeAt(U32 i) const;
	const NODE& const_nodeAt(U32 i) const;

	inline U32 countElements() const { return m_vElements.size();}
	inline U32 countFaces() const {return m_vFaces.size();}
	inline U32 countHalfEdges() const {return m_vHalfEdges.size();}
	inline U32 countEdges() const {return m_vHalfEdges.size() / 2;}
	inline U32 countNodes() const {return m_vNodes.size();}

	//functions
	inline U32 next_hedge(U32 he) const { return m_vHalfEdges[he].next;}
	inline U32 prev_hedge(U32 he) const { return m_vHalfEdges[he].prev;}
	inline U32 opposite_hedge(U32 he) const { return m_vHalfEdges[he].opposite;}

	inline U32 vertex_from_hedge(U32 he) const { return m_vHalfEdges[he].from;}
	inline U32 vertex_to_hedge(U32 he) const {return m_vHalfEdges[he].to;}
	inline U32 halfedge_from_edge(U32 edge, U8 which) const { return edge * 2 + which;}
	inline U32 edge_from_halfedge(U32 he) const { return he / 2;}


	//algorithm
	//splits an edge e at parametric point t
	void displace(double * u);


	//topology modifiers
	bool insert_element(const ELEM& e);
	bool insert_element(U32 nodes[4]);

	//inserting a face will bump the refs on its halfedges
	U32 insert_face(U32 nodes[3]);

	//remove
	void remove_element(U32 i);
	void remove_face(U32 i);

	//erases all objects marked removed
	void garbage_collection();



	/*!
	 * splits the edge in the middle. Adds one new node along the edge and re-connects all half-edges along that.
	 */
	bool split_edge(int idxEdge, double t);

	/*!
	 * cuts an edge completely. Two new nodes are created at the point of cut with no hedges between them.
	 */
	bool cut_edge(int idxEdge, double distance, U32* poutIndexNP0 = NULL, U32* poutIndexNP1 = NULL);


	//algorithmic functions useful for many computational geometry projects
	int getFirstRing(int idxNode, vector<U32>& ringNodes) const;
	int getIncomingHalfEdges(int idxNode, vector<U32>& incomingHE) const;
	int getOutgoingHalfEdges(int idxNode, vector<U32>& outgoingHE) const;

	//checking
	void setElemToShow(U32 elem = INVALID_INDEX);
	U32 getElemToShow() const {return m_elemToShow;}

	//serialize
	bool readVegaFormat(const AnsiStr& strFP);
	bool writeVegaFormat(const AnsiStr& strFP) const;


	//draw
	void draw();
	void drawElement(U32 i) const;

	//aabb
	AABB computeAABB();

	//test that all faces are mapped to a correct key
	bool tst_keys();
private:
	void init();
	inline int insertHEdgeIndexToMap(U32 from, U32 to, U32 idxHE);
	inline int removeHEdgeIndexFromMap(U32 from, U32 to);
	inline bool halfedge_exists(U32 from, U32 to) const;
	U32 halfedge_handle(U32 from, U32 to);

protected:
	U32 m_elemToShow;

	//topology events
	OnNodeEvent m_fOnNodeEvent;
	OnEdgeEvent m_fOnEdgeEvent;
	OnFaceEvent m_fOnFaceEvent;
	OnElementEvent m_fOnElementEvent;

	//containers
	vector<ELEM> m_vElements;
	vector<FACE> m_vFaces;
	vector<HEDGE> m_vHalfEdges;
	vector<NODE> m_vNodes;

	//maps a facekey to the corresponding face handle
	std::map< FaceKey, U32 > m_mapFaces;

	//maps a half-edge from-to pair to the corresponding hedge handle
	std::map< HEdgeKey, U32 > m_mapHalfEdgesIndex;

	//iterators
	typedef std::map< HEdgeKey, U32 >::iterator MAPHEDGEINDEXITER;
	typedef std::map< HEdgeKey, U32 >::iterator MAPHEDGEINDEXCONSTITER;
	typedef std::map< FaceKey, U32 >::iterator MAPFACEITER;
};

}
}

#endif /* TETMESH_H_ */
