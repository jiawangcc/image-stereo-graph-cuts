#include <iostream>     // for cout
#include <random>
#include "Graph2D.h"
#include "cs1037lib-time.h" // for basic timing/pausing
#include <queue> 
using namespace std;
typedef std::tr1::normal_distribution<double> Distribution;
#define pi 3.1415926
#define TERMINAL ( (arc *) 1 )		// COPY OF DEFINITION FROM maxflow.cpp

const int INFTY=100; // value of t-links corresponding to hard-constraints 
int num_model=5;
float con=20.0;
int dp_back, dp_fore;
vector<RGB> mu_fore(num_model),mu_back(num_model),sigma_fore(num_model),sigma_back(num_model);
vector<double> rho_fore(num_model),rho_back(num_model);
vector<vector<double>>Pr_fore(num_model),Pr_back(num_model);


// function for printing error messages from the max-flow library
inline void mf_assert(char * msg) {cout << "Error from max-flow library: " << msg << endl; cin.ignore(); exit(1);}

// function for computing edge weights from intensity differences
//int fn(const double dI, double sigma) {return (int) (5.0/(1.0+(dI*dI/(sigma*sigma))));}  // function used for setting "n-link costs" ("Sensitivity" sigme is a tuning parameter)
int fn(const double dI, double sigma) {return (int) con*exp(-(dI*dI/(sigma*sigma)));} 
//int fn(const double dI, double sigma) {return 1;} 
Graph2D::Graph2D()
: Graph<int,int,int>(0,0,mf_assert)
, m_nodeFlow() 
, m_seeds()
, m_labeling()
{}

Graph2D::Graph2D(Table2D<RGB> & im, double sigma)
: Graph<int,int,int>(im.getWidth()*im.getHeight(),4*im.getWidth()*im.getHeight(),mf_assert)
, m_nodeFlow(im.getWidth(),im.getHeight(),0) 
, m_seeds(im.getWidth(),im.getHeight(),NONE) 
, m_labeling(im.getWidth(),im.getHeight(),NONE)
{
	int wR, wD, height = im.getHeight(), width =  im.getWidth();
	add_node(width*height);    // adding nodes
	for (int y=0; y<(height-1); y++) for (int x=0; x<(width-1); x++) { // adding edges (n-links)
		node_id n = to_node(x,y);
		wR=fn(dI(im[x][y],im[x+1][y]),sigma);   // function dI(...) is declared in Image2D.h
		wD=fn(dI(im[x][y],im[x  ][y+1]),sigma); // function fn(...) is declared at the top of this file
		add_edge( n, n+1,     wR, wR );
		add_edge( n, n+width, wD, wD );
	}
}

void Graph2D::reset(Table2D<RGB> & im, Table2D<RGB> & im2,double sigma, bool eraze_seeds, Mode mode) 
{
	int x, y, wR, wD, wLD, wRD, height=im.getHeight(), width=im.getWidth(),ds,dt;
	Point p,p2;
	m_nodeFlow.reset(width,height,0); // NOTE: width and height of m_nodeFlow table 
	// are used in functions to_node() and to_Point(), 
	// Thus, m_nodeFlow MUST BE RESET FIRST!!!!!!!!
	Graph<int,int,int>::reset();

	if (mode==COLOR){
		vector<vector<int>> bin(512); //number of bin is 8^3
		for (y=0; y<height; y++) for (x=0; x<width; x++){
			int ir=im[x][y].r/32; //every bin contains 256/8 r
			int ig=im[x][y].g/32; //every bin contains 256/8 g
			int ib=im[x][y].b/32; //every bin contains 256/8 b

			/*for (int ib=0; ib<8;ib++)
				for (int ig=0; ig<8;ig++)
					for (int ir=0; ir<8;ir++){
						if((im[x][y].r>=ir*8)&&(im[x][y].r<(ir+1)*8)&&(im[x][y].g>=ig*8)&&(im[x][y].g<(ig+1)*8)&&(im[x][y].b>=ib*8)&&(im[x][y].b<(ib+1)*8))
							bin[ir+ig*8+ib*64].push_back(to_node(x,y));
			}*/
			bin[ir+ig*8+ib*64].push_back(to_node(x,y));
		}

		vector<int> n_bin;
		for (int i=0;i<bin.size();i++){
			if(bin[i].size()>0)
				n_bin.push_back(i);
		}


		add_node(width*height+n_bin.size());    // adding nodes

		for (int i=0; i<n_bin.size(); i++)
			for(int j=0; j<bin[n_bin[i]].size();j++){
				add_edge( width*height+i, bin[n_bin[i]][j], 10, 10);
			}
	}
	else{
		add_node(width*height);
	}

	for (y=0; y<(height-1); y++) for (x=1; x<(width-1); x++) { // adding edges (n-links)
		node_id n = to_node(x,y);
		wR=fn(dI(im[x][y],im[x+1][y]),sigma);   // function dI(...) is declared in Image2D.h
		wD=fn(dI(im[x][y],im[x  ][y+1]),sigma); // function fn(...) is declared at the top of this file
		wLD=fn(dI(im[x][y],im[x-1][y+1]),sigma);
		wRD=fn(dI(im[x][y],im[x+1][y+1]),sigma);
		add_edge( n, n+1,     wR, wR );
		add_edge( n, n+width, wD, wD );	
		add_edge( n, n+width-1, wLD/1.414, wLD/1.414 );	
		add_edge( n, n+width+1, wRD/1.414, wRD/1.414 );	
	}

	m_labeling.reset(width,height,NONE);


	if (mode==SEGMENT){
		cout << "resetting segmentation" << endl;
		if (eraze_seeds) m_seeds.reset(width,height,NONE); // remove hard constraints
		else { // resets hard "constraints" (seeds)
			cout << "keeping seeds" << endl;
			for (p.y=0; p.y<height; p.y++) for (p.x=0; p.x<width; p.x++) { 
				if      (m_seeds[p]==OBJ) add_tweights( p, INFTY, 0   );
				else if (m_seeds[p]==BKG) add_tweights( p,   0, INFTY );
			}
		}
	}

	if (mode==STEREO_GREY||mode==STEREO_COLOR||mode==COLOR){
		cout << "resetting stereo segmentation" << endl;
		for (p.y=0; p.y<height; p.y++) 
			for (p.x=0; p.x<width; p.x++) {
				ds=dt=INFTY;

				for (int i=dp_back+1;i<=dp_fore;i++){  //foreground ds;the closer,the bigger difference is.
					p2=p+Point(i,0);
					//if(im2.pointIn(p2)&&im2.pointIn(p2+Point(0,-1))&&im2.pointIn(p2+Point(0,1))&&im2.pointIn(p2+Point(-1,0))&&im2.pointIn(p2+Point(1,0))){
					if(im2.pointIn(p2)){
						//int dp_temp=abs(dI(im[p],im2[p2]))+abs(dI(im[p],im2[p2+Point(0,-1)]))+abs(dI(im[p],im2[p2+Point(0,1)]))+abs(dI(im[p],im2[p2+Point(-1,0)]))+abs(dI(im[p],im2[p2+Point(1,0)]));
						int dp_temp;
						if (mode==STEREO_GREY) 
							dp_temp=abs(dI(im[p],im2[p2]));
						if (mode==STEREO_COLOR||mode==COLOR) 
							dp_temp=sqrt(pow(double(im[p].r-im2[p2].r),2)+pow(double(im[p].g-im2[p2].g),2)+pow(double(im[p].b-im2[p2].b),2));
						if(dp_temp<ds)
							ds=dp_temp; //choose the minimum
					}
				}

				for (int i=0;i<=dp_back;i++){  //background dt
					p2=p+Point(i,0);
					//if(im2.pointIn(p2)&&im2.pointIn(p2+Point(0,-1))&&im2.pointIn(p2+Point(0,1))&&im2.pointIn(p2+Point(-1,0))&&im2.pointIn(p2+Point(1,0))){
					if(im2.pointIn(p2)){	
						//int dp_temp=abs(dI(im[p],im2[p2]))+abs(dI(im[p],im2[p2+Point(0,-1)]))+abs(dI(im[p],im2[p2+Point(0,1)]))+abs(dI(im[p],im2[p2+Point(-1,0)]))+abs(dI(im[p],im2[p2+Point(1,0)]));
						int dp_temp;
						if (mode==STEREO_GREY) 
							dp_temp=abs(dI(im[p],im2[p2]));
						if (mode==STEREO_COLOR||mode==COLOR) 
							dp_temp=sqrt(pow(double(im[p].r-im2[p2].r),2)+pow(double(im[p].g-im2[p2].g),2)+pow(double(im[p].b-im2[p2].b),2));
						if(dp_temp<dt)
							dt=dp_temp; //choose the minimum 
					}
				}
				add_tweights(p,ds,dt); //add t-link
			}
	}
}

// GUI calls this function when a user left- or right-clicks on an image pixel while in "SEGMENT" mode
void Graph2D::addSeed(Point& p, Label seed_type) 
{
	if (!m_seeds.pointIn(p)) return;
	Label current_constraint = m_seeds[p];
	if (current_constraint==seed_type) return;

	if (current_constraint==NONE) {
		if (seed_type==OBJ) add_tweights( p, INFTY, 0   );
		else                add_tweights( p,   0, INFTY );
	}
	else if (current_constraint==OBJ) {
		if (seed_type==BKG) add_tweights( p, -INFTY, INFTY );
		else                add_tweights( p, -INFTY,   0   );
	}
	else if (current_constraint==BKG) {
		if (seed_type==OBJ) add_tweights( p, INFTY, -INFTY );
		else                add_tweights( p,   0,   -INFTY );
	}
	m_seeds[p]=seed_type;
}

inline Label Graph2D::what_label(Point& p)
{
	node_id i = to_node(p);
	if (nodes[i].parent) return (nodes[i].is_sink) ? BKG : OBJ;
	else                 return NONE;
}

void Graph2D::setLabeling()
{	
	int width  = m_labeling.getWidth();
	int height = m_labeling.getHeight();
	Point p;
	for (p.y=0; p.y<height; p.y++) for (p.x=0; p.x<width; p.x++) m_labeling[p]=what_label(p);
}

int Graph2D::compute_mincut(void (*draw_function)())
{
	int flow = maxflow(draw_function);
	// if visualization of flow is not needed, one can completely remove functions
	// maxflow() and augment() from class Graph2D (t.e. remove their declarations 
	// in Graph2D.h and their implementation code below in Graph2D.cpp) and replace 
	// one line of code above with the following call to the base-class maxflow() method 
	//	      int flow = Graph<int,int,int>::maxflow();

	setLabeling();
	return flow;
}


// overwrites the base class function to allow visualization
// (e.g. calls specified "draw_function" to show each flow augmentation)
int Graph2D::maxflow( void (*draw_function)() ) 
{
	node *i, *j, *current_node = NULL;
	arc *a;
	nodeptr *np, *np_next;

	if (!nodeptr_block)
	{
		nodeptr_block = new DBlock<nodeptr>(NODEPTR_BLOCK_SIZE, error_function);
	}

	changed_list = NULL;
	maxflow_init();

	// main loop
	while ( 1 )
	{
		// test_consistency(current_node);

		if ((i=current_node))
		{
			i -> next = NULL; /* remove active flag */
			if (!i->parent) i = NULL;
		}
		if (!i)
		{
			if (!(i = next_active())) break;
		}

		/* growth */
		if (!i->is_sink)
		{
			/* grow source tree */
			for (a=i->first; a; a=a->next)
			if (a->r_cap)
			{
				j = a -> head;
				if (!j->parent)
				{
					j -> is_sink = 0;
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
					set_active(j);
					add_to_changed_list(j);
				}
				else if (j->is_sink) break;
				else if (j->TS <= i->TS &&
				         j->DIST > i->DIST)
				{
					/* heuristic - trying to make the distance from j to the source shorter */
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
				}
			}
		}
		else
		{
			/* grow sink tree */
			for (a=i->first; a; a=a->next)
			if (a->sister->r_cap)
			{
				j = a -> head;
				if (!j->parent)
				{
					j -> is_sink = 1;
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
					set_active(j);
					add_to_changed_list(j);
				}
				else if (!j->is_sink) { a = a -> sister; break; }
				else if (j->TS <= i->TS &&
				         j->DIST > i->DIST)
				{
					/* heuristic - trying to make the distance from j to the sink shorter */
					j -> parent = a -> sister;
					j -> TS = i -> TS;
					j -> DIST = i -> DIST + 1;
				}
			}
		}

		TIME ++;

		if (a)
		{
			i -> next = i; /* set active flag */
			current_node = i;

			// the line below can be uncommented to visualize current SOURCE and SINK trees
			//... if (draw_function) {setLabeling(); draw_function(); Pause(1);}

			/* augmentation */
			augment(a); // uses overwritten function of the base class (see code below)
			            // that keeps track of flow in table m_nodeFlow
			if (draw_function) {draw_function(); Pause(1);}
			/* augmentation end */

			/* adoption */
			while ((np=orphan_first))
			{
				np_next = np -> next;
				np -> next = NULL;

				while ((np=orphan_first))
				{
					orphan_first = np -> next;
					i = np -> ptr;
					nodeptr_block -> Delete(np);
					if (!orphan_first) orphan_last = NULL;
					if (i->is_sink) process_sink_orphan(i);
					else            process_source_orphan(i);
				}

				orphan_first = np_next;
			}
			/* adoption end */
		}
		else current_node = NULL;
	}
	// test_consistency();

	delete nodeptr_block; 
	nodeptr_block = NULL; 

	maxflow_iteration ++;
	return flow;
}

// overwrites the base class function (to save the nodeFlow in a 2D array m_nodeFlow)
void Graph2D::augment(arc *middle_arc)
{
	node *i;
	arc *a;
	int bottleneck;


	/* 1. Finding bottleneck capacity */
	/* 1a - the source tree */
	bottleneck = middle_arc -> r_cap;
	for (i=middle_arc->sister->head; ; i=a->head)
	{
		a = i -> parent;
		if (a == TERMINAL) break;
		if (bottleneck > a->sister->r_cap) bottleneck = a -> sister -> r_cap;
	}
	if (bottleneck > i->tr_cap) bottleneck = i -> tr_cap;
	/* 1b - the sink tree */
	for (i=middle_arc->head; ; i=a->head)
	{
		a = i -> parent;
		if (a == TERMINAL) break;
		if (bottleneck > a->r_cap) bottleneck = a -> r_cap;
	}
	if (bottleneck > - i->tr_cap) bottleneck = - i -> tr_cap;


	/* 2. Augmenting */
	/* 2a - the source tree */
	middle_arc -> sister -> r_cap += bottleneck;
	middle_arc -> r_cap -= bottleneck;
	for (i=middle_arc->sister->head; ; i=a->head)
	{
		m_nodeFlow[to_point((node_id)(i-nodes))] = 1;
		a = i -> parent;
		if (a == TERMINAL) break;
		a -> r_cap += bottleneck;
		a -> sister -> r_cap -= bottleneck;
		if (!a->sister->r_cap)
		{
			set_orphan_front(i); // add i to the beginning of the adoption list
		}
	}
	i -> tr_cap -= bottleneck;
	if (!i->tr_cap)
	{
		set_orphan_front(i); // add i to the beginning of the adoption list
	}
	/* 2b - the sink tree */
	for (i=middle_arc->head; ; i=a->head)
	{
		m_nodeFlow[to_point((node_id)(i-nodes))] = 1;
		a = i -> parent;
		if (a == TERMINAL) break;
		a -> sister -> r_cap += bottleneck;
		a -> r_cap -= bottleneck;
		if (!a->r_cap)
		{
			set_orphan_front(i); // add i to the beginning of the adoption list
		}
	}
	i -> tr_cap += bottleneck;
	if (!i->tr_cap)
	{
		set_orphan_front(i); // add i to the beginning of the adoption list
	}


	flow += bottleneck;
}

