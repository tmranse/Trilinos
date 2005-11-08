/*
#@HEADER
# ************************************************************************
#
#                 Copyright (2002) Sandia Corporation
#
# Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
# license for use of this work by or on behalf of the U.S. Government.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# Questions? Contact Jonathan Hu (jhu@sandia.gov) or Ray Tuminaro 
# (rstumin@sandia.gov).
#
# ************************************************************************
#@HEADER
*/
/* ******************************************************************** */
/* See the file COPYRIGHT for a complete copyright notice, contact      */
/* person and disclaimer.                                               */
/* ******************************************************************** */
#ifdef TRILINOS_PACKAGE


#include "mrtr_interface.H"
#include "mrtr_projector.H"
#include "mrtr_utils.H"
#include "mrtr_pnode.H"
#include "mrtr_segment_bilineartri.H"


/*----------------------------------------------------------------------*
 |  ctor (public)                                            mwgee 06/05|
 *----------------------------------------------------------------------*/
MRTR::Interface::Interface(int Id,  bool oneD, Epetra_Comm& comm, int outlevel) :
Id_(Id),
outlevel_(outlevel),
oneD_(oneD),
isComplete_(false),
isIntegrated_(false),
gcomm_(comm),
lcomm_(null),
mortarside_(-1),
ptype_(MRTR::Interface::proj_none),
primal_(MRTR::Function::func_none),
dual_(MRTR::Function::func_none)
{
  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       mwgee 06/05|
 *----------------------------------------------------------------------*/
MRTR::Interface::Interface(MRTR::Interface& old) :
Id_(old.Id_),
outlevel_(old.outlevel_),
oneD_(old.oneD_),
isComplete_(old.isComplete_),
isIntegrated_(old.isIntegrated_),
gcomm_(old.gcomm_),
mortarside_(old.mortarside_),
ptype_(old.ptype_),
primal_(old.primal_),
dual_(old.dual_)
{
  // copy the nodes and segments
  for (int i=0; i<2; ++i)
  {
    // the local segment map
    map<int,RefCountPtr<MRTR::Segment> >::const_iterator seg_curr;
    for (seg_curr=old.seg_[i].begin(); seg_curr != old.seg_[i].end(); ++seg_curr)
    {
      RefCountPtr<MRTR::Segment>  tmpseg = rcp(seg_curr->second->Clone());
      seg_[i].insert(pair<int,RefCountPtr<MRTR::Segment> >(tmpseg->Id(),tmpseg));
    }
    // the global segment map
    for (seg_curr=old.rseg_[i].begin(); seg_curr != old.rseg_[i].end(); ++seg_curr)
    {
      RefCountPtr<MRTR::Segment> tmpseg = rcp(seg_curr->second->Clone());
      rseg_[i].insert(pair<int,RefCountPtr<MRTR::Segment> >(tmpseg->Id(),tmpseg));
    }
    // the local node map
    map<int,RefCountPtr<MRTR::Node> >::const_iterator node_curr;
    for (node_curr=old.node_[i].begin(); node_curr != old.node_[i].end(); ++node_curr)
    {
      RefCountPtr<MRTR::Node> tmpnode = rcp( new MRTR::Node(*(node_curr->second)));
      node_[i].insert(pair<int,RefCountPtr<MRTR::Node> >(tmpnode->Id(),tmpnode));
    }
    // the global node map
    for (node_curr=old.rnode_[i].begin(); node_curr != old.rnode_[i].end(); ++node_curr)
    {
      RefCountPtr<MRTR::Node> tmpnode = rcp(new MRTR::Node(*(node_curr->second)));
      rnode_[i].insert(pair<int,RefCountPtr<MRTR::Node> >(tmpnode->Id(),tmpnode));
    }
  }
  // copy the PID maps
  segPID_  = old.segPID_;
  nodePID_ = old.nodePID_;
  
  // copy the local communicator of this interface
  lcomm_ = old.lcomm_;
    
  // rebuild the node-segment topology on this new interface
  BuildNodeSegmentTopology(); 
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                            mwgee 06/05|
 *----------------------------------------------------------------------*/
MRTR::Interface::~Interface()
{ 
  // delete segments
  for (int i=0; i<2; ++i)
  {
    seg_[i].clear();
    rseg_[i].clear();
  } 
  
  // delete nodes
  for (int i=0; i<2; ++i)
  {
    node_[i].clear();
    rnode_[i].clear();
  } 
  
  // delete PID maps
  segPID_.clear();
  nodePID_.clear();
}

/*----------------------------------------------------------------------*
 |  print segments of this interface to cout                  (public)  |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::PrintSegments() const
{ 
  if (!lComm()) return true;
  
  map<int,RefCountPtr<MRTR::Segment> >::const_iterator curr;
  for (int j=0; j<2; ++j) 
  {
    for (int k=0; k<lComm()->NumProc(); ++k) 
    {
      if (lComm()->MyPID()==k)
      {
        cout << "---global/local Proc " << gcomm_.MyPID() << "/" << k 
             << ":\t Segments Side " << j << endl;
        for (curr=rseg_[j].begin(); curr!=rseg_[j].end(); ++curr)
        {
          RefCountPtr<MRTR::Segment> seg = curr->second;
          if (SegPID(seg->Id()) == k)
          {
            if (seg == null)
            {
              cout << "***ERR*** MRTR::Interface::PrintSegments:\n"
                   << "***ERR*** found NULL entry in map of segments\n"
                   << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
              return false;
            }
            cout << *seg;
          }
        }
      }
      lComm()->Barrier();
    }
  }
  lComm()->Barrier();
  return true;
}

/*----------------------------------------------------------------------*
 |  print nodes of this interface to cout                     (public)  |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::PrintNodes() const
{ 
  if (!lComm()) return true;
  
  map<int,RefCountPtr<MRTR::Node> >::const_iterator curr;
  
  for (int j=0; j<2; ++j)
  {
    for (int k=0; k<lComm()->NumProc(); ++k)
    {
      if (lComm()->MyPID()==k)
      {
        cout << "---global/local Proc " << gcomm_.MyPID() << "/" << k  
             << ":\t Nodes Side " << j << endl;
        for (curr=rnode_[j].begin(); curr!=rnode_[j].end(); ++curr)
        {
          RefCountPtr<MRTR::Node> node = curr->second;
          if (NodePID(node->Id()) == k)
          {
            if (node == null)
            {
              cout << "***ERR*** MRTR::Interface::PrintNodes:\n"
                   << "***ERR*** found NULL entry in map of nodes\n"
                   << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
              return false;
            }
            cout << *node;
          }
        }
        lComm()->Barrier();
      }
    }
  }
  lComm()->Barrier();
  return true;
}

/*----------------------------------------------------------------------*
 |  print interface to cout                                   (public)  |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::Print() const
{ 
  
  if (!IsComplete())
  {
    if (gcomm_.MyPID()==0)
    {
      cout << "Complete() was NOT called\n";
      cout << "Cannot print node/segment info\n";
    }
    return true;
  }
  if (!lComm()) return true;
  if (gcomm_.MyPID()==0)
  {
    cout << "===== MRTR Interface # " << Id() << " =====\n";
    if (oneD_)
      cout << "Dimension: 1D\n";
    else
      cout << "Dimension: 2D\n";
    if (GetProjectionType()==MRTR::Interface::proj_none)
      cout << "ProjectionType: none\n";
    else if (GetProjectionType()==MRTR::Interface::proj_continousnormalfield)
      cout << "ProjectionType: continousnormalfield\n";
    else if (GetProjectionType()==MRTR::Interface::proj_orthogonal)
      cout << "ProjectionType: orthogonal\n";
  }
  
  if (gcomm_.MyPID()==0)
    cout << "----- Segments -----\n";
  fflush(stdout);
  PrintSegments();

  if (gcomm_.MyPID()==0)
    cout << "----- Nodes    -----\n";
  fflush(stdout);
  PrintNodes();

  return true;
}

/*----------------------------------------------------------------------*
 |  << operator                                              mwgee 06/05|
 *----------------------------------------------------------------------*/
ostream& operator << (ostream& os, const MRTR::Interface& inter)
{ 
  inter.Print();
  return (os);
}

/*----------------------------------------------------------------------*
 |  add a single segment to a specified side of the interface (public)  |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::AddSegment(MRTR::Segment& seg, int side)
{ 
  // check whether this interface has been finalized before
  if (IsComplete())
  {
    if (OutLevel()>0)
      cout << "***ERR*** MRTR::Interface::AddSegment:\n"
           << "***ERR*** Cannot add segment as Complete() was called before\n"
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  
  // check side
  if (side != 0 && side != 1)
  {
    if (OutLevel()>0)
      cout << "***ERR*** MRTR::Interface::AddSegment:\n"
           << "***ERR*** parameter side: " << side << " has to be 0 or 1\n"
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  
  if (seg.Type()==MRTR::Segment::seg_BiLinearQuad)
  {
    if (seg.Nnode() != 4)
    {
      cout << "***ERR*** MRTR::Interface::AddSegment:\n"
           << "***ERR*** Unknown number of nodes " << seg.Nnode() << "for BilinearQuad\n"
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
      return false;
    }
    // split the quad into 2 triangles
    int ids1[3];
    ids1[0] = seg.NodeIds()[0];
    ids1[1] = seg.NodeIds()[1];
    ids1[2] = seg.NodeIds()[2];
    int ids2[3];
    ids2[0] = seg.NodeIds()[0];
    ids2[1] = seg.NodeIds()[2];
    ids2[2] = seg.NodeIds()[3];

    // create 2 triangles, give second one the negative id
    RefCountPtr<MRTR::Segment> tmp1 = rcp( new MRTR::Segment_BiLinearTri(seg.Id(),3,ids1));
    RefCountPtr<MRTR::Segment> tmp2 = rcp( new MRTR::Segment_BiLinearTri(-seg.Id(),3,ids2));
    
    // add 2 triangles
    map<int,RefCountPtr<MRTR::Segment> >* s = 0;
    if (side==0) s = &(seg_[0]);
    else         s = &(seg_[1]);
    s->insert(pair<int,RefCountPtr<MRTR::Segment> >(tmp1->Id(),tmp1));    
    s->insert(pair<int,RefCountPtr<MRTR::Segment> >(tmp2->Id(),tmp2));    
  }
  else // all other types of segments
  {
    // copy the segment
    RefCountPtr<MRTR::Segment> tmp = rcp( seg.Clone());
  
    // add segment
    map<int,RefCountPtr<MRTR::Segment> >* s = 0;
    if (side==0) s = &(seg_[0]);
    else         s = &(seg_[1]);
    s->insert(pair<int,RefCountPtr<MRTR::Segment> >(tmp->Id(),tmp));
  }

  return true;
}

/*----------------------------------------------------------------------*
 |  add a single node to a specified side of the interface (public)     |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::AddNode(MRTR::Node& node, int side)
{ 
  // check whether this interface has been finalized before
  if (IsComplete())
  {
    if (OutLevel()>0)
      cout << "***ERR*** MRTR::Interface::AddNode:\n"
           << "***ERR*** Cannot add node as Complete() was called before\n"
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  
  // check side
  if (side != 0 && side != 1)
  {
    if (OutLevel()>0)
      cout << "***ERR*** MRTR::Interface::AddNode:\n"
           << "***ERR*** parameter side: " << side << " has to be 0 or 1\n"
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  
  // copy the node
  RefCountPtr<MRTR::Node>  tmp = rcp( new MRTR::Node(node));
  
  // add node
  map<int,RefCountPtr<MRTR::Node> >* n = 0;
  if (side==0) n = &(node_[0]);
  else         n = &(node_[1]);
  n->insert(pair<int,RefCountPtr<MRTR::Node> >(tmp->Id(),tmp));

  return true;
}

/*----------------------------------------------------------------------*
 |  set a MRTR::function derived class to all segments                  |
 |  on a specified side                                                 |
 |  side      (in)    side of interface to set function to              |
 |  id        (in)    id of the function                                |
 |  func      (in)    ptr to function class to set to segments          |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::SetFunctionAllSegmentsSide(int side, 
                                                 int id, MRTR::Function* func)
{ 
  if (side!=0 && side!=1)
  {
    cout << "***ERR*** MRTR::Interface::SetFunctionAllSegmentsSide:\n"
         << "***ERR*** side = " << side << " not equal 0 or 1\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  if (id<0)
  {
    cout << "***ERR*** MRTR::Interface::SetFunctionAllSegmentsSide:\n"
         << "***ERR*** id = " << id << " < 0 (out of range)\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  if (!func)
  {
    cout << "***ERR*** MRTR::Interface::SetFunctionAllSegmentsSide:\n"
         << "***ERR*** func = NULL on input\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  
  // set the function to my own segments
  map<int,RefCountPtr<MRTR::Segment> >::iterator scurr;
  for (scurr=seg_[side].begin(); scurr!=seg_[side].end(); ++scurr)
    scurr->second->SetFunction(id,func);

  // if redundant segments are already build, set function there as well
  for (scurr=rseg_[side].begin(); scurr!=rseg_[side].end(); ++scurr)
    scurr->second->SetFunction(id,func);
  
  
  return true;
}

/*----------------------------------------------------------------------*
 |  set the Mortar (Master) side of the interface                       |
 |                                                                      |
 |  NOTE: This is a collective call that returns global number of       |
 |        segments of a side over all procs                             |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::SetMortarSide(int side)
{ 
  if (side!=0 && side!=1 && side!=-2)
  {
    cout << "***ERR*** MRTR::Interface::SetMortarSide:\n"
         << "***ERR*** side = " << side << " not equal 0 or 1\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    mortarside_=-1;
    return false;
  }
  mortarside_ = side;
  return true;
}

/*----------------------------------------------------------------------*
 |  get number of segments on interface side 0 or 1                     |
 |  side (in) side of which the total number of segments to return      |
 |                                                                      |
 |  NOTE: This is a collective call that returns global number of       |
 |        segments of a side over all procs                             |
 |        participating in the intra-communcicator of this interface.   |
 |        It returns 0 for procs not part of that intra-communcicator   |
 |        Complete() needs to be called before using this method        |
 *----------------------------------------------------------------------*/
int MRTR::Interface::GlobalNsegment(int side)
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GlobalNsegment:\n"
         << "***ERR*** Complete() was not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return -1;
  }
  if (side!=0 && side!=1)
  {
    cout << "***ERR*** MRTR::Interface::GlobalNsegment:\n"
         << "***ERR*** side = " << side << " not equal 0 or 1\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  if (!lComm()) 
    return 0;
  int lnsegment = seg_[side].size();
  int gnsegment;
  lcomm_->SumAll(&lnsegment,&gnsegment,1);
  return(gnsegment);
}

/*----------------------------------------------------------------------*
 |  get number of segments on interface (both sides)                    |
 |  NOTE: This is a collective call that returns global number of       |
 |        segments over all procs                                       |
 |        participating in the intra-communcicator of this interface.   |
 |        It returns 0 for procs not part of that intra-communcicator   |
 |        Complete() needs to be called before using this method        |
 *----------------------------------------------------------------------*/
int MRTR::Interface::GlobalNsegment()
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GlobalNsegment:\n"
         << "***ERR*** Complete() was not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return -1;
  }
  if (!lComm()) 
    return 0;
  int nsegment = rseg_[0].size() + rseg_[1].size();
  return(nsegment);
}

/*----------------------------------------------------------------------*
 |  get number of nodes on interface side 0 or 1                     |
 |  side (in) side of which the total number of segments to return      |
 |                                                                      |
 |  NOTE: This is a collective call that returns global number of       |
 |        segments of a side over all procs                             |
 |        participating in the intra-communcicator of this interface.   |
 |        It returns 0 for procs not part of that intra-communcicator   |
 |        Complete() needs to be called before using this method        |
 *----------------------------------------------------------------------*/
int MRTR::Interface::GlobalNnode(int side)
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GlobalNnode:\n"
         << "***ERR*** Complete() was not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return -1;
  }
  if (side!=0 && side!=1)
  {
    cout << "***ERR*** MRTR::Interface::GlobalNnode:\n"
         << "***ERR*** side = " << side << " not equal 0 or 1\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  if (!lComm()) 
    return 0;
  int gnnode = rnode_[side].size();
  return(gnnode);
}

/*----------------------------------------------------------------------*
 |  get number of nodes on interface (both sides)                       |
 |  NOTE: This is a collective call that returns global number of       |
 |        segments over all procs                                       |
 |        participating in the intra-communcicator of this interface.   |
 |        It returns 0 for procs not part of that intra-communcicator   |
 |        Complete() needs to be called before using this method        |
 *----------------------------------------------------------------------*/
int MRTR::Interface::GlobalNnode()
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GlobalNnode:\n"
         << "***ERR*** Complete() was not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return -1;
  }
  if (!lComm()) 
    return 0;
  int gnnode = rnode_[0].size() + rnode_[1].size();;
  return(gnnode);
}

/*----------------------------------------------------------------------*
 |  find PID (process id) for given nodal id nid                        |
 |  NOTE:                                                               |
 |  this method returns the PID from the intra-communicator of this     |
 |  interface. If called from a proc that is not part of this           |
 |  intra-communicator, the method returns -1                           |
 *----------------------------------------------------------------------*/
int MRTR::Interface::NodePID(int nid) const
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::NodePID:\n"
         << "***ERR*** Cannot search node, Complete() not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  if (!lComm())
  {
    cout << "***WRN*** MRTR::Interface::NodePID:\n"
         << "***WRN*** Proc " << gcomm_.MyPID() << " not part of intra-communicator of interface " << Id() << "\n"
         << "***WRN*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  
  map<int,int>::const_iterator curr = nodePID_.find(nid);
  if (curr != nodePID_.end())
    return(curr->second);
  else
  {
    cout << "***ERR*** MRTR::Interface::NodePID:\n"
         << "***ERR*** Proc/Intra-Proc " << gcomm_.MyPID() << "/" << lcomm_->MyPID() << ": Cannot find node " << nid << " on interface " << Id() << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return(-1);
  }
}

/*----------------------------------------------------------------------*
 |  find PID (process id) for given segment id sid                      |
 *----------------------------------------------------------------------*/
int MRTR::Interface::SegPID(int sid) const
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::SegPID:\n"
         << "***ERR*** Cannot search segment, Complete() not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  if (!lComm())
  {
    cout << "***WRN*** MRTR::Interface::NodePID:\n"
         << "***WRN*** Proc " << gcomm_.MyPID() << " not part of intra-communicator of interface " << Id() << "\n"
         << "***WRN*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  
  map<int,int>::const_iterator curr = segPID_.find(sid);
  if (curr != segPID_.end())
    return(curr->second);
  else
  {
    cout << "***ERR*** MRTR::Interface::SegPID:\n"
         << "***ERR*** Proc/Intra-Proc " << gcomm_.MyPID() << "/"<< lcomm_->MyPID() << ": Cannot find segment " << sid << "on interface " << Id() << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return(-1);
  }
}

/*----------------------------------------------------------------------*
 |  find PID (process id) for given segment id sid                      |
 *----------------------------------------------------------------------*/
int MRTR::Interface::OtherSide(int side)
{ 
  if (side==0) return 1;
  else if (side==1) return 0;
  else
  {
    cout << "***ERR*** MRTR::Interface::OtherSide:\n"
         << "***ERR*** side " << side << " out of range (0 or 1)\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return -1;
  }
}

/*----------------------------------------------------------------------*
 |  get view of a local node with node id nid                           |
 |  if sid is not a local node will return NULL                         |
 *----------------------------------------------------------------------*/
RefCountPtr<MRTR::Node> MRTR::Interface::GetNodeViewLocal(int nid)
{ 
  map<int,RefCountPtr<MRTR::Node> >::iterator curr = node_[0].find(nid);
  if (curr != node_[0].end())
    return(curr->second);
  curr = node_[1].find(nid);
  if (curr != node_[1].end())
    return(curr->second);
  return (null);
}

/*----------------------------------------------------------------------*
 |  get view of a node with node id nid                                 |
 *----------------------------------------------------------------------*/
RefCountPtr<MRTR::Node> MRTR::Interface::GetNodeView(int nid)
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GetNodeView:\n"
         << "***ERR*** Interface " << Id() << ": Complete() not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  if (!lComm()) return null;
  
  map<int,RefCountPtr<MRTR::Node> >::iterator curr = rnode_[0].find(nid);
  if (curr != rnode_[0].end())
    return(curr->second);
  curr = rnode_[1].find(nid);
  if (curr != rnode_[1].end())
    return(curr->second);
  return (null);
}

/*----------------------------------------------------------------------*
 |  get view of ALL nodes on this interface                             |
 |  method allocates a ptr vector the calling methos is in charge of    |
 | destroying it                                                        |
 | returns NULL if proc is not art of the local communicator            |
 *----------------------------------------------------------------------*/
MRTR::Node** MRTR::Interface::GetNodeView()
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GetNodeView:\n"
         << "***ERR*** Interface " << Id() << ": Complete() not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  if (!lComm()) return NULL;
  
  MRTR::Node** view = new MRTR::Node*[GlobalNnode()];
  int count=0;
  map<int,RefCountPtr<MRTR::Node> >::iterator curr;
  for (int i=0; i<2; ++i)
    for (curr=rnode_[i].begin(); curr != rnode_[i].end(); ++curr)
    {
      view[count] = curr->second.get();
      ++count;
    }
  return view;
}

/*----------------------------------------------------------------------*
 |  get view of a local segment with id sid                             |
 *----------------------------------------------------------------------*/
RefCountPtr<MRTR::Segment>  MRTR::Interface::GetSegmentView(int sid)
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GetSegmentView:\n"
         << "***ERR*** Interface " << Id() << ": Complete() not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  if (!lComm()) return null;
  
  map<int,RefCountPtr<MRTR::Segment> >::iterator curr = rseg_[0].find(sid);
  if (curr != rseg_[0].end())
    return(curr->second);
  curr = rseg_[1].find(sid);
  if (curr != rseg_[1].end())
    return(curr->second);
  return (null);
}

/*----------------------------------------------------------------------*
 |  get view of ALL segments on this interface                          |
 |  method returns a ptr to a vector, calling method is in              |
 | charge of deleteing it                                               |
 *----------------------------------------------------------------------*/
MRTR::Segment** MRTR::Interface::GetSegmentView()
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GetSegmentView:\n"
         << "***ERR*** Interface " << Id() << ": Complete() not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  if (!lComm()) return NULL;
  
  MRTR::Segment** segs = new MRTR::Segment*[GlobalNsegment()];
  map<int,RefCountPtr<MRTR::Segment> >::iterator curr;
  int count=0;
  for (int i=0; i<2; ++i)
    for (curr=rseg_[i].begin(); curr != rseg_[i].end(); ++curr)
    {
      segs[count] = curr->second.get();
      ++count;
    }
  return segs;
}

/*----------------------------------------------------------------------*
 |  find out which side a segment is on                                 |
 | returns -1 if it can't find the segment on either side               |
 *----------------------------------------------------------------------*/
int MRTR::Interface::GetSide(MRTR::Segment* seg)
{ 
  if (!lComm()) return -1;
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GetSide:\n"
         << "***ERR*** Interface " << Id() << ": Complete() not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  if (!lComm())
  {
    cout << "***ERR*** MRTR::Interface::GetSide:\n"
         << "***ERR*** Interface " << Id() << ": Proc " << gcomm_.MyPID() << "not in intra-comm\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  map<int,RefCountPtr<MRTR::Segment> >::iterator curr = rseg_[0].find(seg->Id());
  if (curr != rseg_[0].end())
    return(0);
  curr = rseg_[1].find(seg->Id());
  if (curr != rseg_[1].end())
    return(1);
  return (-1);
}

/*----------------------------------------------------------------------*
 |  find out which side a node is on                                    |
 | returns -1 if it can't find the node on either side                  |
 *----------------------------------------------------------------------*/
int MRTR::Interface::GetSide(MRTR::Node* node)
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GetSide:\n"
         << "***ERR*** Interface " << Id() << ": Complete() not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  if (!lComm())
  {
    cout << "***ERR*** MRTR::Interface::GetSide:\n"
         << "***ERR*** Interface " << Id() << ": Proc " << gcomm_.MyPID() << "not in intra-comm\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  map<int,RefCountPtr<MRTR::Node> >::iterator curr = rnode_[0].find(node->Id());
  if (curr != rnode_[0].end())
    return(0);
  curr = rnode_[1].find(node->Id());
  if (curr != rnode_[1].end())
    return(1);
  return (-1);
}

/*----------------------------------------------------------------------*
 |  find out which side a node is on                                    |
 | returns -1 if it can't find the node on either side                  |
 *----------------------------------------------------------------------*/
int MRTR::Interface::GetSide(int nodeid)
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::GetSide:\n"
         << "***ERR*** Interface " << Id() << ": Complete() not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  if (!lComm())
  {
    cout << "***ERR*** MRTR::Interface::GetSide:\n"
         << "***ERR*** Interface " << Id() << ": Proc " << gcomm_.MyPID() << "not in intra-comm\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);
  }
  map<int,RefCountPtr<MRTR::Node> >::iterator curr = rnode_[0].find(nodeid);
  if (curr != rnode_[0].end())
    return(0);
  curr = rnode_[1].find(nodeid);
  if (curr != rnode_[1].end())
    return(1);
  return (-1);
}

/*----------------------------------------------------------------------*
 | allreduce all segments of a side                                     |
 |                                                                      |
 | NOTE: this is a collective call of all procs in the intra-comm       |
 |       After call to RedundantNodes and RedundantSegments             |
 |       a call to BuildNodeSegmentTopology is necessary to complete    |
 |       the construction of redundant nodes/segments
 *----------------------------------------------------------------------*/
bool MRTR::Interface::RedundantSegments(int side)
{ 
  if (side != 0 && side != 1)
  {
    cout << "***ERR*** MRTR::Interface::RedundantSegments:\n"
         << "***ERR*** side=" << side << " out of range (0 or 1)\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::RedundantSegments:\n"
         << "***ERR*** Complete() not called on interface " << Id() << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  
  // send everybody who doesn't belong here out of here
  if (!lComm())
    return true;
  
  map<int,RefCountPtr<MRTR::Segment> >* rmap = &(rseg_[side]);
  // check whether redundant map has been build before
  if (rmap->size() != 0)
    return true;

  // add my own segments to the redundant map
  map<int,RefCountPtr<MRTR::Segment> >::const_iterator curr;
  for (curr=seg_[side].begin(); curr != seg_[side].end(); ++curr)
  {
    //MRTR::Segment* tmp = curr->second->Clone();
    // FIXME: is this ok? it's not a deep copy anymore.....
    RefCountPtr<MRTR::Segment> tmp = curr->second;
    rmap->insert(pair<int,RefCountPtr<MRTR::Segment> >(curr->first,tmp));
  }
  
  // loop over all procs and broadcast proc's segments
  for (int proc=0; proc<lcomm_->NumProc(); ++proc)
  {
    int nseg = 0;
    if (proc==lcomm_->MyPID())
      nseg = MyNsegment(side);
    lcomm_->Broadcast(&nseg,1,proc);

    int  bsize = nseg*12;
    vector<int> bcast; 
    
    // pack proc's segments
    if (proc==lcomm_->MyPID())
    {
       int  count = 0;
       bcast.resize(bsize);
       for (curr=seg_[side].begin(); curr != seg_[side].end(); ++curr)
       {
         int  numint;
         int* spack = curr->second->Pack(&numint);
         if (count+numint>=bsize)
         {
           bsize += 5* numint;
           bcast.resize(bsize);
         }
         for (int i=0; i<numint; ++i)
           bcast[count++] = spack[i];
         delete [] spack;
       }
       bsize = count;
    }
    
    // broadcast proc's segments
    lcomm_->Broadcast(&bsize,1,proc);
    if (lcomm_->MyPID() != proc)
      bcast.resize(bsize);
    lcomm_->Broadcast(&bcast[0],bsize,proc);
    
    // Unpack proc's segments
    if (lcomm_->MyPID() != proc)
    {
      int count=0;
      for (int i=0; i<nseg; ++i)
      {
        // the type of segment is stored second in the pack
	MRTR::Segment* tmp = AllocateSegment(bcast[count+1]);
        tmp->UnPack(&(bcast[count]));
        RefCountPtr<MRTR::Segment> tmp2 = rcp(tmp);
        count += bcast[count];
        rmap->insert(pair<int,RefCountPtr<MRTR::Segment> >(tmp2->Id(),tmp2));
      }
    }
    bcast.clear();
  } // for (int proc=0; proc<lcomm_->NumProc(); ++proc)
  return true;
}


/*----------------------------------------------------------------------*
 | allreduce all nodes of a side                                        |
 |                                                                      |
 | NOTE: this is a collective call of all procs in the intra-comm       |
 |       After call to RedundantNodes and RedundantSegments             |
 |       a call to BuildNodeSegmentTopology is necessary to complete    |
 |       the construction of redundant nodes/segments
 *----------------------------------------------------------------------*/
bool MRTR::Interface::RedundantNodes(int side)
{ 
  if (side != 0 && side != 1)
  {
    cout << "***ERR*** MRTR::Interface::RedundantNodes:\n"
         << "***ERR*** side=" << side << " out of range (0 or 1)\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::RedundantNodes:\n"
         << "***ERR*** Complete() not called on interface " << Id() << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (-1);
  }
  
  // send everybody who doesn't belong here out of here
  if (!lComm())
    return true;

  map<int,RefCountPtr<MRTR::Node> >* rmap = &(rnode_[side]);
  // check whether redundant map has been build before
  if (rmap->size() != 0)
    return true;

  // add my own nodes to the redundant map
  map<int,RefCountPtr<MRTR::Node> >::const_iterator curr;
  for (curr=node_[side].begin(); curr != node_[side].end(); ++curr)
  {
    //MRTR::Node* tmp = new MRTR::Node(*(curr->second));
    //FIXME: this is not a deep copy anymore. Is this ok?
    RefCountPtr<MRTR::Node> tmp = curr->second;
    rmap->insert(pair<int,RefCountPtr<MRTR::Node> >(curr->first,tmp));
  }
  
  // loop all procs and broadcast proc's nodes
  for (int proc=0; proc<lcomm_->NumProc(); ++proc)
  {
    int nnode = 0;
    if (proc==lcomm_->MyPID())
      nnode = MyNnode(side);
    lcomm_->Broadcast(&nnode,1,proc);
    
    int bsize = nnode*3;
    vector<double> bcast;
    
    // pack proc's nodes
    if (proc==lcomm_->MyPID())
    {
      int count = 0;
      bcast.resize(bsize);
      for (curr=node_[side].begin(); curr != node_[side].end(); ++curr)
      {
        int numdouble;
        double* npack = curr->second->Pack(&numdouble);
        if (count+numdouble>=bsize)
        {
          bsize += 3*numdouble;
          bcast.resize(bsize);
        }
        for (int i=0; i<numdouble; ++i)
          bcast[count++] = npack[i];
        delete [] npack;
      }
      bsize = count;
    }
    
    // bcast proc's nodes
    lcomm_->Broadcast(&bsize,1,proc);
    if (lcomm_->MyPID() != proc)
      bcast.resize(bsize);
    lcomm_->Broadcast(&bcast[0],bsize,proc);
    
    // Unpack proc's nodes
    if (lcomm_->MyPID() != proc)
    {
      int count=0;
      for (int i=0; i<nnode; ++i)
      {
        RefCountPtr<MRTR::Node> tmp = rcp(new MRTR::Node());
        tmp->UnPack(&(bcast[count]));
        count += (int)bcast[count];
        rmap->insert(pair<int,RefCountPtr<MRTR::Node> >(tmp->Id(),tmp));
      }
    }    
    bcast.clear();
  } // for (int proc=0; proc<lcomm_->NumProc(); ++proc)
  return true;
}

/*----------------------------------------------------------------------*
 | (re)build the topology info between nodes and segments               |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::BuildNodeSegmentTopology()
{ 
  if (!IsComplete())
  {
    cout << "***WRN*** MRTR::Interface::BuildNodeSegmentTopology:\n"
         << "***WRN*** Complete() not called on interface " << Id() << "\n"
         << "***WRN*** Cannot build node<->segment topology\n"
         << "***WRN*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  
  if (!lComm()) return true;
  
  // loop nodes and find their adjacent segments
  map<int,RefCountPtr<MRTR::Node> >::iterator ncurr;
  for (int side=0; side<2; ++side)
  {
    for (ncurr=rnode_[side].begin(); ncurr != rnode_[side].end(); ++ncurr)
      ncurr->second->GetPtrstoSegments(*this);
  }
  
  // loop segments and find their adjacent nodes
  map<int,RefCountPtr<MRTR::Segment> >::iterator scurr;
  for (int side=0; side<2; ++side)
  {
    for (scurr=rseg_[side].begin(); scurr != rseg_[side].end(); ++scurr)
      scurr->second->GetPtrstoNodes(*this);
  }
  return true;
}

/*----------------------------------------------------------------------*
 | set lagrange multiplier dofs starting from minLMGID                  |
 | to all slave nodes or segments that have a projection                |
 | On exit, return maxLMGID+1, where maxLMGID is the last LM dof number |
 | used here                                                            |
 | Note that this is collective for ALL procs                           |
 *----------------------------------------------------------------------*/
int MRTR::Interface::SetLMDofs(int minLMGID)
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::SetLMDofs:\n"
         << "***ERR*** Complete() was not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (0);
  }

  if (lComm())
  {
    // FIXME: this is different for discontinous lagrange multipliers
  
    int mside = MortarSide();
    int sside = OtherSide(mside);
    
    // loop nodes on slave side and set LMdofs for those who have a projection
    map<int,RefCountPtr<MRTR::Node> >::iterator curr;
    for (curr=rnode_[sside].begin(); curr!=rnode_[sside].end(); ++curr)
    {
      RefCountPtr<MRTR::Node> node = curr->second;
      
      // check whether this node has a projection
      RefCountPtr<MRTR::ProjectedNode> pnode = node->GetProjectedNode();
      if (pnode==null) continue;
      
      
      // get number of dofs on this node to choose the same number of dofs
      // for the LM
      int ndof = node->Ndof();
      
      // set LM dofs to this node and it's projection
      for (int i=0; i<ndof; ++i)
      {
        node->SetLagrangeMultiplierId(minLMGID+i);
        pnode->SetLagrangeMultiplierId(minLMGID+i);
      }
      minLMGID += ndof;
    } // for (curr=rnode_[sside].begin(); curr!=rnode_[sside].end(); ++curr)
  } // if (lComm())
  
  // broadcast minLMGID to all procs including those not in intra-comm
  int lbcaster = 0;
  int gbcaster = 0;
  if (lComm())
    if (lComm()->MyPID()==0)
      lbcaster = gcomm_.MyPID();
  gcomm_.MaxAll(&lbcaster,&gbcaster,1);
  gcomm_.Broadcast(&minLMGID,1,gbcaster);
  return(minLMGID);
}


/*----------------------------------------------------------------------*
 | retrieve a vector containing a list of lm ids owned by this processor|
 | The calling routine is responsible for destroying this list          |
 *----------------------------------------------------------------------*/
vector<int>* MRTR::Interface::MyLMIds()
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::MyLMIds:\n"
         << "***ERR*** Complete() was not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return (0);
  }

  int mside = MortarSide();
  int sside = OtherSide(mside);

  // allocate a vector with a guess
  vector<int>* lmids = new vector<int>;
  
  // procs not in intra-comm return an empty vector
  if (!lComm())
  {
    lmids->resize(0);
    return lmids;
  }
  
  lmids->resize(rnode_[sside].size()*10);
  int count=0;
    
  map<int,RefCountPtr<MRTR::Node> >::iterator curr;
  for (curr=rnode_[sside].begin(); curr!=rnode_[sside].end(); ++curr)
  {
    RefCountPtr<MRTR::Node> node = curr->second;
    if (NodePID(node->Id()) != lComm()->MyPID()) 
      continue;
    int  nlmdof = node->Nlmdof();
    if (!nlmdof) 
      continue; 
    const int* ids = node->LMDof();
    if (count+nlmdof>lmids->size())
      lmids->resize(lmids->size()+50*nlmdof);
    for (int i=0; i<nlmdof; ++i)
      (*lmids)[count++] = ids[i];
  }
  lmids->resize(count);


  return lmids;
}

/*----------------------------------------------------------------------*
 | detect end segments and reduce the order of the lm shape functions   |
 | on these end segments                                                |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::DetectEndSegmentsandReduceOrder()
{ 
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::DetectEndSegmentsandReduceOrder:\n"
         << "***ERR*** Complete() was not called on interface " << Id_ << "\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }
  if (!lComm()) return true;

  int mside = MortarSide();
  int sside = OtherSide(mside);

  if (IsOneDimensional())
  {
    /*
    in the 1D interface case, an end segment is detected as follows:
    - an end segments is attached to a node that has a projection and is the 
      ONLY segment attached to that node
    - an end segment is attached to a node that has a pseudo projection (that is
      a node carrying lagrange mutlipliers but not having a projection) 
    */
    
    // loop all nodes on the slave side and find those with only one segment
    map<int,RefCountPtr<MRTR::Node> >::iterator curr;
    for (curr=rnode_[sside].begin(); curr!=rnode_[sside].end(); ++curr)
    {
      RefCountPtr<MRTR::Node> node = curr->second;
      bool foundit = false;
      if (node->Nseg()<2)
        foundit = true;
      if (node->GetProjectedNode() != null)
        if (!(node->GetProjectedNode()->Segment()))
          foundit = true;
      if (!foundit)
        continue;
      
      MRTR::Segment** segs = node->Segments();

      for (int i=0; i<node->Nseg(); ++i)
      {
        MRTR::Function::FunctionType type = 
          segs[i]->FunctionType(1);
          
        MRTR::Function_Constant1D* tmp1;  
        switch (type)
        {
          // for linear and dual linear reduce function order to constant
          case MRTR::Function::func_Constant1D:
          case MRTR::Function::func_Linear1D:
          case MRTR::Function::func_DualLinear1D:
            tmp1 = new Function_Constant1D();
            segs[i]->SetFunction(1,tmp1);
          break;
          case MRTR::Function::func_none:
            cout << "***ERR*** MRTR::Interface::DetectEndSegmentsandReduceOrder:\n"
                 << "***ERR*** interface " << Id() << " function type of function 1 on segment " << segs[0]->Id() << " is func_none\n"
                 << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
            exit(EXIT_FAILURE);     
          break;
          default:
            cout << "***ERR*** MRTR::Interface::DetectEndSegmentsandReduceOrder:\n"
                 << "***ERR*** interface " << Id() << " function type of function 1 on segment " << segs[0]->Id() << " is unknown\n"
                 << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
            exit(EXIT_FAILURE);     
          break;
        } // switch (type)
      } // for (int i=0; i<node->Nseg(); ++i)
    } // for (curr=rnode_[sside].begin(); curr!=rnode_[sside].end(); ++curr)

  } // if (IsOneDimensional())
  else
  {
    cout << "***ERR*** MRTR::Interface::DetectEndSegmentsandReduceOrder:\n"
         << "***ERR*** not impl. for 2D interfaces\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    return false;
  }

  return true;
}

/*----------------------------------------------------------------------*
 | set the type of shape functions that are supposed to be used         |
 | primal: type of shape function for the trace space                   |
 | dual:   type of shape function for the LM space                      |
 *----------------------------------------------------------------------*/
bool MRTR::Interface::SetFunctionTypes(MRTR::Function::FunctionType primal,
                                       MRTR::Function::FunctionType dual)
{ 
  primal_ = primal;
  dual_   = dual;
  return true;
}

/*----------------------------------------------------------------------*
 | set functions to all segments depending on the variables primal_     |
 | and dual_
 *----------------------------------------------------------------------*/
bool MRTR::Interface::SetFunctionsFromFunctionTypes()
{ 
  if (!lComm()) return true;
  if (!IsComplete())
  {
    cout << "***ERR*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
         << "***ERR*** interface " << Id() << " : Complete() was not called\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);     
  }
  if (dual_==MRTR::Function::func_none)
  {
    cout << "***ERR*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
         << "***ERR*** interface " << Id() << " : no dual function type set\n"
         << "***ERR*** use SetFunctionTypes(..) to set function types\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);     
  }
  
  // set the primal shape functions
  MRTR::Function_Linear1D*     func1 = NULL;
  MRTR::Function_Constant1D*   func2 = NULL;
  MRTR::Function_DualLinear1D* func3 = NULL;
  switch(primal_)
  {
    case MRTR::Function::func_Linear1D:
      func1 = new MRTR::Function_Linear1D();
      SetFunctionAllSegmentsSide(0,0,func1);
      SetFunctionAllSegmentsSide(1,0,func1);
    break;
    case MRTR::Function::func_DualLinear1D:
      cout << "***WRN*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
           << "***WRN*** interface " << Id() << " : setting discontious dual shape functions as\n"
           << "***WRN*** primal isoparametric trace space function is probably a bad idea...\n"
           << "***WRN*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    break;
    case MRTR::Function::func_Constant1D:
      cout << "***WRN*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
           << "***WRN*** interface " << Id() << " : setting constant shape functions as\n"
           << "***WRN*** primal isoparametric trace space function is probably a bad idea...\n"
           << "***WRN*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    break;
    case MRTR::Function::func_none:
      cout << "***ERR*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
           << "***ERR*** interface " << Id() << " : no primal function type set\n"
           << "***ERR*** use SetFunctionTypes(..) to set function types\n"
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
      exit(EXIT_FAILURE);     
    break;
    default:
      cout << "***ERR*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
           << "***ERR*** interface " << Id() << " : Unknown function type: " << primal_ << endl
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
      exit(EXIT_FAILURE);     
    break;
  }
  
  int side = MortarSide();
  if (side != 1 && side != 0)
  {
    cout << "***ERR*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
         << "***ERR*** interface " << Id() << " : Mortar Side not set set\n"
         << "***ERR*** use SetMortarSide(int side) to choose mortar side first\n"
         << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
    exit(EXIT_FAILURE);     
  }
  side = OtherSide(side); // we want the slave side here

  switch(dual_)
  {
    case MRTR::Function::func_Linear1D:
      func1 = new MRTR::Function_Linear1D();
      SetFunctionAllSegmentsSide(side,1,func1);
    break;
    case MRTR::Function::func_DualLinear1D:
      func3 = new MRTR::Function_DualLinear1D();
      SetFunctionAllSegmentsSide(side,1,func3);
    break;
    case MRTR::Function::func_Constant1D:
      func2 = new MRTR::Function_Constant1D();
      SetFunctionAllSegmentsSide(side,1,func2);
    break;
    case MRTR::Function::func_none:
      cout << "***ERR*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
           << "***ERR*** interface " << Id() << " : no dual function type set\n"
           << "***ERR*** use SetFunctionTypes(..) to set function types\n"
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
      exit(EXIT_FAILURE);     
    break;
    default:
      cout << "***ERR*** MRTR::Interface::SetFunctionsFromFunctionTypes:\n"
           << "***ERR*** interface " << Id() << " : Unknown function type: " << dual_ << endl
           << "***ERR*** file/line: " << __FILE__ << "/" << __LINE__ << "\n";
      exit(EXIT_FAILURE);     
    break;
  }

  return true;
}

#endif // TRILINOS_PACKAGE
