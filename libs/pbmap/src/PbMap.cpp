/* +---------------------------------------------------------------------------+
   |                 The Mobile Robot Programming Toolkit (MRPT)               |
   |                                                                           |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2013, Individual contributors, see AUTHORS file        |
   | Copyright (c) 2005-2013, MAPIR group, University of Malaga                |
   | Copyright (c) 2012-2013, University of Almeria                            |
   | All rights reserved.                                                      |
   |                                                                           |
   | Redistribution and use in source and binary forms, with or without        |
   | modification, are permitted provided that the following conditions are    |
   | met:                                                                      |
   |    * Redistributions of source code must retain the above copyright       |
   |      notice, this list of conditions and the following disclaimer.        |
   |    * Redistributions in binary form must reproduce the above copyright    |
   |      notice, this list of conditions and the following disclaimer in the  |
   |      documentation and/or other materials provided with the distribution. |
   |    * Neither the name of the copyright holders nor the                    |
   |      names of its contributors may be used to endorse or promote products |
   |      derived from this software without specific prior written permission.|
   |                                                                           |
   | THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       |
   | 'AS IS' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED |
   | TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR|
   | PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE |
   | FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL|
   | DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR|
   |  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)       |
   | HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,       |
   | STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN  |
   | ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           |
   | POSSIBILITY OF SUCH DAMAGE.                                               |
   +---------------------------------------------------------------------------+ */

/*  Plane-based Map (PbMap) library
 *  Construction of plane-based maps and localization in it from RGBD Images.
 *  Writen by Eduardo Fernandez-Moral. See docs for <a href="group__mrpt__pbmap__grp.html" >mrpt-pbmap</a>
 */
#include <mrpt/pbmap.h> // precomp. hdr

#include <mrpt/utils/CStream.h>
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>

using namespace mrpt::utils;
using namespace mrpt::pbmap;


IMPLEMENTS_SERIALIZABLE(PbMap, CSerializable, pbmap)

/*---------------------------------------------------------------
	Constructor
  ---------------------------------------------------------------*/
PbMap::PbMap() :
    FloorPlane(-1),
    globalMapPtr( new pcl::PointCloud<pcl::PointXYZRGBA>() ),
    edgeCloudPtr(new pcl::PointCloud<pcl::PointXYZRGBA>),
    outEdgeCloudPtr(new pcl::PointCloud<pcl::PointXYZRGBA>)
{
}

/*---------------------------------------------------------------
						writeToStream
 ---------------------------------------------------------------*/
void  PbMap::writeToStream(CStream &out, int *out_Version) const
{
	if (out_Version)
		*out_Version = 0;
	else
	{
		// The data
		uint32_t n = uint32_t( vPlanes.size() );
		out << n;
		for (uint32_t i=0; i < n; i++)
			out << vPlanes[i];
	}
}

/*---------------------------------------------------------------
						readFromStream
 ---------------------------------------------------------------*/
void  PbMap::readFromStream(CStream &in, int version)
{
	switch(version)
	{
	case 0:
    {
        // Delete previous content:
        vPlanes.clear();

        // The data
        // First, write the number of planes:
        uint32_t	n;
        in >> n;
        vPlanes.resize(n);
          for (uint32_t i=0; i < n; i++)
          {
            Plane pl;
            pl.id = i;
            in >> pl;
            vPlanes[i] = pl;
          }
    } break;
	default:
		MRPT_THROW_UNKNOWN_SERIALIZATION_VERSION(version)
	};
}

void PbMap::savePbMap(string filePath)
{
  // Serialize PbMap
  mrpt::utils::CFileGZOutputStream serialize_pbmap(filePath + "/planes.pbmap");
  serialize_pbmap << *this;
  serialize_pbmap.close();

  // Save reconstructed point cloud
  pcl::io::savePCDFile(filePath + "/cloud.pcd", *this->globalMapPtr);
}

// Merge two pbmaps
void PbMap::MergeWith(PbMap &pbm, Eigen::Matrix4f &T)
{
  // Rotate and translate PbMap
  for(size_t i = 0; i < pbm.vPlanes.size(); i++)
  {
    Plane &plane = pbm.vPlanes[i];

    // Transform normal and ppal direction
    plane.v3normal = T.block(0,0,3,3) * plane.v3normal;
    plane.v3PpalDir = T.block(0,0,3,3) * plane.v3PpalDir;

    // Transform centroid
    plane.v3center = T.block(0,0,3,3) * plane.v3center + T.block(0,3,3,1);

    // Transform convex hull points
    pcl::transformPointCloud(*plane.polygonContourPtr, *plane.polygonContourPtr, T);

    vPlanes.push_back(plane);
  }

  // Rotate and translate the point cloud
  pcl::PointCloud<pcl::PointXYZRGBA>::Ptr alignedPointCloud(new pcl::PointCloud<pcl::PointXYZRGBA>);
  pcl::transformPointCloud(*pbm.globalMapPtr,*alignedPointCloud,T);

  *globalMapPtr += *alignedPointCloud;

}

#include <fstream>
// Print PbMap content to a text file
void PbMap::printPbMap(string txtFilePbm)
{
cout << "PbMap 0.2\n\n";

  ofstream pbm;
  pbm.open(txtFilePbm.c_str());
  pbm << "PbMap 0.2\n\n";
  pbm << "MapPlanes " << vPlanes.size() << endl;
  for(unsigned i=0; i < vPlanes.size(); i++)
  {
    pbm << " ID " << vPlanes[i].id << " obs " << vPlanes[i].numObservations;
    pbm << " areaVoxels " << vPlanes[i].areaVoxels << " areaHull " << vPlanes[i].areaHull;
    pbm << " ratioXY " << vPlanes[i].elongation << " structure " << vPlanes[i].bFromStructure << " label " << vPlanes[i].label;
    pbm << "\n normal\n" << vPlanes[i].v3normal << "\n center\n" << vPlanes[i].v3center;
    pbm << "\n PpalComp\n" << vPlanes[i].v3PpalDir << "\n RGB\n" << vPlanes[i].v3colorNrgb;
    pbm << "\n Neighbors (" << vPlanes[i].neighborPlanes.size() << "): ";
    for(map<unsigned,unsigned>::iterator it=vPlanes[i].neighborPlanes.begin(); it != vPlanes[i].neighborPlanes.end(); it++)
      pbm << it->first << " ";
    pbm << "\n CommonObservations: ";
    for(map<unsigned,unsigned>::iterator it=vPlanes[i].neighborPlanes.begin(); it != vPlanes[i].neighborPlanes.end(); it++)
      pbm << it->second << " ";
    pbm << "\n ConvexHull (" << vPlanes[i].polygonContourPtr->size() << "): \n";
    for(unsigned j=0; j < vPlanes[i].polygonContourPtr->size(); j++)
      pbm << "\t" << vPlanes[i].polygonContourPtr->points[j].x << " " << vPlanes[i].polygonContourPtr->points[j].y << " " << vPlanes[i].polygonContourPtr->points[j].z << endl;
    pbm << endl;
  }
  pbm.close();
}
