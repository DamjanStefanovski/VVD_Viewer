//
//  For more information, please see: http://software.sci.utah.edu
//
//  The MIT License
//
//  Copyright (c) 2004 Scientific Computing and Imaging Institute,
//  University of Utah.
//
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#include <math.h>

#include <FLIVR/TextureBrick.h>
#include <FLIVR/TextureRenderer.h>
#include <FLIVR/Utils.h>
#include <wx/wx.h>
#include <wx/url.h>
#include <utility>
#include <iostream>
#include <jpeglib.h>
#include <libcurl\curl\curl.h>

using namespace std;

namespace FLIVR
{
   TextureBrick::TextureBrick (Nrrd* n0, Nrrd* n1,
         int nx, int ny, int nz, int nc, int* nb,
         int ox, int oy, int oz,
         int mx, int my, int mz,
         const BBox& bbox, const BBox& tbox, const BBox& dbox, int findex, long long offset, long long fsize)
      : nx_(nx), ny_(ny), nz_(nz), nc_(nc), ox_(ox), oy_(oy), oz_(oz),
      mx_(mx), my_(my), mz_(mz), bbox_(bbox), tbox_(tbox), dbox_(dbox), findex_(findex), offset_(offset), fsize_(fsize)
   {
      for (int i=0; i<TEXTURE_MAX_COMPONENTS; i++)
      {
         data_[i] = 0;
         nb_[i] = 0;
         ntype_[i] = TYPE_NONE;
      }

      for (int c=0; c<nc_; c++)
      {
         nb_[c] = nb[c];
      }
      if (nc_==1)
      {
         ntype_[0] = TYPE_INT;
      }
      else if (nc_>1)
      {
         ntype_[0] = TYPE_INT_GRAD;
         ntype_[1] = TYPE_GM;
      }

	  compute_edge_rays(bbox_);
      compute_edge_rays_tex(tbox_);

      data_[0] = n0;
      data_[1] = n1;

      nmask_ = -1;

      //if it's been drawn in a full update loop
      for (int i=0; i<TEXTURE_RENDER_MODES; i++)
         drawn_[i] = false;

      //priority
      priority_ = 0;

	  brkdata_ = NULL;
	  id_in_loadedbrks = -1;

   }

   TextureBrick::~TextureBrick()
   {
      // Creator of the brick owns the nrrds.
      // This object never deletes that memory.
      data_[0] = 0;
      data_[1] = 0;

	  if (brkdata_) delete [] brkdata_;
   }

   /* The cube is numbered in the following way

Corners:

2________6        y
/|        |        |
/ |       /|        |
/  |      / |        |
/   0_____/__4        |
3---------7   /        |_________ x
|  /      |  /         /
| /       | /         /
|/        |/         /
1_________5         /
z

Edges:

,____1___,        y
/|        |        |
/ 0       /|        |
9  |     10 2        |
/   |__3__/__|        |
/_____5___/   /        |_________ x
|  /      | 11         /
4 8       6 /         /
|/        |/         /
|____7____/         /
z
    */

   void TextureBrick::compute_edge_rays(BBox &bbox)
   {
      // set up vertices
      Point corner[8];
      corner[0] = bbox.min();
      corner[1] = Point(bbox.min().x(), bbox.min().y(), bbox.max().z());
      corner[2] = Point(bbox.min().x(), bbox.max().y(), bbox.min().z());
      corner[3] = Point(bbox.min().x(), bbox.max().y(), bbox.max().z());
      corner[4] = Point(bbox.max().x(), bbox.min().y(), bbox.min().z());
      corner[5] = Point(bbox.max().x(), bbox.min().y(), bbox.max().z());
      corner[6] = Point(bbox.max().x(), bbox.max().y(), bbox.min().z());
      corner[7] = bbox.max();

      // set up edges
      edge_[0] = Ray(corner[0], corner[2] - corner[0]);
      edge_[1] = Ray(corner[2], corner[6] - corner[2]);
      edge_[2] = Ray(corner[4], corner[6] - corner[4]);
      edge_[3] = Ray(corner[0], corner[4] - corner[0]);
      edge_[4] = Ray(corner[1], corner[3] - corner[1]);
      edge_[5] = Ray(corner[3], corner[7] - corner[3]);
      edge_[6] = Ray(corner[5], corner[7] - corner[5]);
      edge_[7] = Ray(corner[1], corner[5] - corner[1]);
      edge_[8] = Ray(corner[0], corner[1] - corner[0]);
      edge_[9] = Ray(corner[2], corner[3] - corner[2]);
      edge_[10] = Ray(corner[6], corner[7] - corner[6]);
      edge_[11] = Ray(corner[4], corner[5] - corner[4]);
   }

   void TextureBrick::compute_edge_rays_tex(BBox &bbox)
   {
      // set up vertices
      Point corner[8];
      corner[0] = bbox.min();
      corner[1] = Point(bbox.min().x(), bbox.min().y(), bbox.max().z());
      corner[2] = Point(bbox.min().x(), bbox.max().y(), bbox.min().z());
      corner[3] = Point(bbox.min().x(), bbox.max().y(), bbox.max().z());
      corner[4] = Point(bbox.max().x(), bbox.min().y(), bbox.min().z());
      corner[5] = Point(bbox.max().x(), bbox.min().y(), bbox.max().z());
      corner[6] = Point(bbox.max().x(), bbox.max().y(), bbox.min().z());
      corner[7] = bbox.max();

      // set up edges
      tex_edge_[0] = Ray(corner[0], corner[2] - corner[0]);
      tex_edge_[1] = Ray(corner[2], corner[6] - corner[2]);
      tex_edge_[2] = Ray(corner[4], corner[6] - corner[4]);
      tex_edge_[3] = Ray(corner[0], corner[4] - corner[0]);
      tex_edge_[4] = Ray(corner[1], corner[3] - corner[1]);
      tex_edge_[5] = Ray(corner[3], corner[7] - corner[3]);
      tex_edge_[6] = Ray(corner[5], corner[7] - corner[5]);
      tex_edge_[7] = Ray(corner[1], corner[5] - corner[1]);
      tex_edge_[8] = Ray(corner[0], corner[1] - corner[0]);
      tex_edge_[9] = Ray(corner[2], corner[3] - corner[2]);
      tex_edge_[10] = Ray(corner[6], corner[7] - corner[6]);
      tex_edge_[11] = Ray(corner[4], corner[5] - corner[4]);
   }

   // compute polygon of edge plane intersections
   void TextureBrick::compute_polygon(Ray& view, double t,
         vector<double>& vertex, vector<double>& texcoord,
         vector<int>& size)
   {
      compute_polygons(view, t, t, 1.0, vertex, texcoord, size);
   }

   //set timin_, timax_, dt_ and vray_
   void TextureBrick::compute_t_index_min_max(Ray& view, double dt)
   {
      Point corner[8];
      corner[0] = bbox_.min();
      corner[1] = Point(bbox_.min().x(), bbox_.min().y(), bbox_.max().z());
      corner[2] = Point(bbox_.min().x(), bbox_.max().y(), bbox_.min().z());
      corner[3] = Point(bbox_.min().x(), bbox_.max().y(), bbox_.max().z());
      corner[4] = Point(bbox_.max().x(), bbox_.min().y(), bbox_.min().z());
      corner[5] = Point(bbox_.max().x(), bbox_.min().y(), bbox_.max().z());
      corner[6] = Point(bbox_.max().x(), bbox_.max().y(), bbox_.min().z());
      corner[7] = bbox_.max();

      double tmin = Dot(corner[0] - view.origin(), view.direction());
      double tmax = tmin;
      int maxi = 0;
	  int mini = 0;
      for (int i=1; i<8; i++)
      {
         double t = Dot(corner[i] - view.origin(), view.direction());
         if (t < tmin) { mini = i; tmin = t; }
         if (t > tmax) { maxi = i; tmax = t; }
      }

	  bool order = TextureRenderer::get_update_order();
	  double tanchor;
	  tanchor = Dot(corner[mini], view.direction());
	  timin_ = (int)floor(tanchor/dt)+1;
	  tanchor = Dot(corner[maxi], view.direction());
	  timax_ = (int)floor(tanchor/dt);
	  dt_ = dt;
	  vray_ = view;
   }

   void TextureBrick::compute_polygons(Ray& view, double dt,
         vector<double>& vertex, vector<double>& texcoord,
         vector<int>& size)
   {
      Point corner[8];
      corner[0] = bbox_.min();
      corner[1] = Point(bbox_.min().x(), bbox_.min().y(), bbox_.max().z());
      corner[2] = Point(bbox_.min().x(), bbox_.max().y(), bbox_.min().z());
      corner[3] = Point(bbox_.min().x(), bbox_.max().y(), bbox_.max().z());
      corner[4] = Point(bbox_.max().x(), bbox_.min().y(), bbox_.min().z());
      corner[5] = Point(bbox_.max().x(), bbox_.min().y(), bbox_.max().z());
      corner[6] = Point(bbox_.max().x(), bbox_.max().y(), bbox_.min().z());
      corner[7] = bbox_.max();

      double tmin = Dot(corner[0] - view.origin(), view.direction());
      double tmax = tmin;
      int maxi = 0;
	  int mini = 0;
      for (int i=1; i<8; i++)
      {
         double t = Dot(corner[i] - view.origin(), view.direction());
         if (t < tmin) { mini = i; tmin = t; }
         if (t > tmax) { maxi = i; tmax = t; }
      }

      // Make all of the slices consistent by offsetting them to a fixed
      // position in space (the origin).  This way they are consistent
      // between bricks and don't change with camera zoom.
/*      double tanchor = Dot(corner[maxi], view.direction());
      double tanchor0 = floor(tanchor/dt)*dt;
      double tanchordiff = tanchor - tanchor0;
      tmax -= tanchordiff;
*/
	  bool order = TextureRenderer::get_update_order();
	  double tanchor = Dot(corner[order?mini:maxi], view.direction());
	  double tanchor0 = (floor(tanchor/dt)+1)*dt;
	  double tanchordiff = tanchor - tanchor0;
	  if (order) tmin -= tanchordiff;
	  else tmax -= tanchordiff;

      compute_polygons(view, tmin, tmax, dt, vertex, texcoord, size);
   }

   // compute polygon list of edge plane intersections
   //
   // This is never called externally and could be private.
   //
   // The representation returned is not efficient, but it appears a
   // typical rendering only contains about 1k triangles.
   void TextureBrick::compute_polygons(Ray& view,
         double tmin, double tmax, double dt,
         vector<double>& vertex, vector<double>& texcoord,
         vector<int>& size)
   {
      Vector vv[12], tt[12]; // temp storage for vertices and texcoords

      int k = 0, degree = 0;

      // find up and right vectors
      Vector vdir = view.direction();
      view_vector_ = vdir;
      Vector up;
      Vector right;
      switch(MinIndex(fabs(vdir.x()),
               fabs(vdir.y()),
               fabs(vdir.z())))
      {
      case 0:
         up.x(0.0); up.y(-vdir.z()); up.z(vdir.y());
         break;
      case 1:
         up.x(-vdir.z()); up.y(0.0); up.z(vdir.x());
         break;
      case 2:
         up.x(-vdir.y()); up.y(vdir.x()); up.z(0.0);
         break;
      }
      up.normalize();
      right = Cross(vdir, up);
      bool order = TextureRenderer::get_update_order();
      for (double t = order?tmin:tmax;
            order?(t <= tmax):(t >= tmin);
            t += order?dt:-dt)
      {
         // we compute polys back to front
         // find intersections
         degree = 0;
         for (size_t j=0; j<12; j++)
         {
            double u;

            FLIVR::Vector vec = -view.direction();
            FLIVR::Point pnt = view.parameter(t);
            bool intersects = edge_[j].planeIntersectParameter
               (vec, pnt, u);
            if (intersects && u >= 0.0 && u <= 1.0)
            {
               Point p;
               p = edge_[j].parameter(u);
               vv[degree] = (Vector)p;
               p = tex_edge_[j].parameter(u);
               tt[degree] = (Vector)p;
               degree++;
            }
         }

         if (degree < 3 || degree >6) continue;

         if (degree > 3)
         {
            // compute centroids
            Vector vc(0.0, 0.0, 0.0), tc(0.0, 0.0, 0.0);
            for (int j=0; j<degree; j++)
            {
               vc += vv[j]; tc += tt[j];
            }
            vc /= (double)degree; tc /= (double)degree;

            // sort vertices
            int idx[6];
            double pa[6];
            for (int i=0; i<degree; i++)
            {
               double vx = Dot(vv[i] - vc, right);
               double vy = Dot(vv[i] - vc, up);

               // compute pseudo-angle
               pa[i] = vy / (fabs(vx) + fabs(vy));
               if (vx < 0.0) pa[i] = 2.0 - pa[i];
               else if (vy < 0.0) pa[i] = 4.0 + pa[i];
               // init idx
               idx[i] = i;
            }
            Sort(pa, idx, degree);

            // output polygon
            for (int j=0; j<degree; j++)
            {
               vertex.push_back(vv[idx[j]].x());
               vertex.push_back(vv[idx[j]].y());
               vertex.push_back(vv[idx[j]].z());
               texcoord.push_back(tt[idx[j]].x());
               texcoord.push_back(tt[idx[j]].y());
               texcoord.push_back(tt[idx[j]].z());
            }
         }
         else if (degree == 3)
         {
            // output a single triangle
            for (int j=0; j<degree; j++)
            {
               vertex.push_back(vv[j].x());
               vertex.push_back(vv[j].y());
               vertex.push_back(vv[j].z());
               texcoord.push_back(tt[j].x());
               texcoord.push_back(tt[j].y());
               texcoord.push_back(tt[j].z());
            }
         }
         k += degree;
         size.push_back(degree);
      }
   }

   void TextureBrick::get_polygon(int tid, int &size, double* &v, double* &t)
   {
	   bool order = TextureRenderer::get_update_order();
	   int id = (order ? tid - timin_ : timax_ - tid);
	   unsigned int offset = size_integ_[id];
	   
	   size = size_[id];
	   t = &texcoord_[offset*3];
       v = &vertex_[offset*3];
   }
   
   void TextureBrick::clear_polygons()
   {
	   if (!vertex_.empty()) vertex_.clear();
	   if (!texcoord_.empty()) texcoord_.clear();
	   if (!size_.empty()) size_.clear();
	   if (!size_integ_.empty()) size_integ_.clear();
   }

   void TextureBrick::compute_polygons2()
   {
	  clear_polygons();

      Vector vv[12], tt[12]; // temp storage for vertices and texcoords

      int k = 0, degree = 0;

      // find up and right vectors
      Vector vdir = vray_.direction();
      view_vector_ = vdir;
      Vector up;
      Vector right;
      switch(MinIndex(fabs(vdir.x()),
               fabs(vdir.y()),
               fabs(vdir.z())))
      {
      case 0:
         up.x(0.0); up.y(-vdir.z()); up.z(vdir.y());
         break;
      case 1:
         up.x(-vdir.z()); up.y(0.0); up.z(vdir.x());
         break;
      case 2:
         up.x(-vdir.y()); up.y(vdir.x()); up.z(0.0);
         break;
      }
      up.normalize();
      right = Cross(vdir, up);
      bool order = TextureRenderer::get_update_order();
      for (int t = order?timin_:timax_;
            order?(t <= timax_):(t >= timin_);
            t += order?1:-1)
      {
         // we compute polys back to front
         // find intersections
         degree = 0;
         for (size_t j=0; j<12; j++)
         {
            double u;

            FLIVR::Vector vec = -vray_.direction();
            FLIVR::Point pnt = vray_.parameter(t*dt_);
            bool intersects = edge_[j].planeIntersectParameter
               (vec, pnt, u);
            if (intersects && u >= 0.0 && u <= 1.0)
            {
               Point p;
               p = edge_[j].parameter(u);
               vv[degree] = (Vector)p;
               p = tex_edge_[j].parameter(u);
               tt[degree] = (Vector)p;
               degree++;
            }
         }

         if (degree < 3 || degree >6) continue;

         if (degree > 3)
         {
            // compute centroids
            Vector vc(0.0, 0.0, 0.0), tc(0.0, 0.0, 0.0);
            for (int j=0; j<degree; j++)
            {
               vc += vv[j]; tc += tt[j];
            }
            vc /= (double)degree; tc /= (double)degree;

            // sort vertices
            int idx[6];
            double pa[6];
            for (int i=0; i<degree; i++)
            {
               double vx = Dot(vv[i] - vc, right);
               double vy = Dot(vv[i] - vc, up);

               // compute pseudo-angle
               pa[i] = vy / (fabs(vx) + fabs(vy));
               if (vx < 0.0) pa[i] = 2.0 - pa[i];
               else if (vy < 0.0) pa[i] = 4.0 + pa[i];
               // init idx
               idx[i] = i;
            }
            Sort(pa, idx, degree);

            // output polygon
            for (int j=0; j<degree; j++)
            {
               vertex_.push_back(vv[idx[j]].x());
               vertex_.push_back(vv[idx[j]].y());
               vertex_.push_back(vv[idx[j]].z());
               texcoord_.push_back(tt[idx[j]].x());
               texcoord_.push_back(tt[idx[j]].y());
               texcoord_.push_back(tt[idx[j]].z());
            }
         }
         else if (degree == 3)
         {
            // output a single triangle
            for (int j=0; j<degree; j++)
            {
               vertex_.push_back(vv[j].x());
               vertex_.push_back(vv[j].y());
               vertex_.push_back(vv[j].z());
               texcoord_.push_back(tt[j].x());
               texcoord_.push_back(tt[j].y());
               texcoord_.push_back(tt[j].z());
            }
         }
         k += degree;
         size_.push_back(degree);
		 size_integ_.push_back(k);
      }
   }

   int TextureBrick::sx()
   {
      if (data_[0]->dim == 3)
         return (int)data_[0]->axis[0].size;
      else
         return (int)data_[0]->axis[1].size;
   }

   int TextureBrick::sy()
   {
      if (data_[0]->dim == 3)
         return (int)data_[0]->axis[1].size;
      else
         return (int)data_[0]->axis[2].size;
   }

   int TextureBrick::sz()
   {
      if (data_[0]->dim == 3)
         return (int)data_[0]->axis[2].size;
      else
         return (int)data_[0]->axis[3].size;
   }

   GLenum TextureBrick::tex_type_aux(Nrrd* n)
   {
      // GL_BITMAP!?
      if (n->type == nrrdTypeChar)   return GL_BYTE;
      if (n->type == nrrdTypeUChar)  return GL_UNSIGNED_BYTE;
      if (n->type == nrrdTypeShort)  return GL_SHORT;
      if (n->type == nrrdTypeUShort) return GL_UNSIGNED_SHORT;
      if (n->type == nrrdTypeInt)    return GL_INT;
      if (n->type == nrrdTypeUInt)   return GL_UNSIGNED_INT;
      if (n->type == nrrdTypeFloat)  return GL_FLOAT;
      return GL_NONE;
   }

   size_t TextureBrick::tex_type_size(GLenum t)
   {
      if (t == GL_BYTE)           { return sizeof(GLbyte); }
      if (t == GL_UNSIGNED_BYTE)  { return sizeof(GLubyte); }
      if (t == GL_SHORT)          { return sizeof(GLshort); }
      if (t == GL_UNSIGNED_SHORT) { return sizeof(GLushort); }
      if (t == GL_INT)            { return sizeof(GLint); }
      if (t == GL_UNSIGNED_INT)   { return sizeof(GLuint); }
      if (t == GL_FLOAT)          { return sizeof(GLfloat); }
      return 0;
   }

   GLenum TextureBrick::tex_type(int c)
   {
      if (c < nc_)
         return tex_type_aux(data_[c]);
      else if (c == nmask_)
         return GL_UNSIGNED_BYTE;
      else if (c == nlabel_)
         return GL_UNSIGNED_INT;
      else
         return GL_NONE;
   }

   void *TextureBrick::tex_data(int c)
   {
	   if (c >= 0 && data_[c])
	   {
		   if(data_[c]->data)
		   {
			   unsigned char *ptr = (unsigned char *)(data_[c]->data);
			   long long offset = (long long)(oz()) * (long long)(sx()) * (long long)(sy()) +
								  (long long)(oy()) * (long long)(sx()) +
								  (long long)(ox());
			   return ptr + offset * tex_type_size(tex_type(c));
		   }
	   }

	   return NULL;
   }

   void *TextureBrick::tex_data_brk(int c, wstring *fname, int filetype, bool useURL)
   {
	   unsigned char *ptr = NULL;
	   if(brkdata_) ptr = (unsigned char *)(brkdata_);
	   else
	   {
		   int bd = tex_type_size(tex_type(c));
		   ptr = new unsigned char[nx_*ny_*nz_*bd];
		   if (!read_brick((char *)ptr, nx_*ny_*nz_*bd, fname, filetype, useURL))
		   {
			   delete [] ptr;
			   return NULL;
		   }
		   brkdata_ = (void *)ptr;
	   }
	   return ptr;
   }
   
   void TextureBrick::set_priority()
   {
      if (!data_[0])
      {
         priority_ = 0;
         return;
      }
      size_t vs = tex_type_size(tex_type(0));
      size_t sx = data_[0]->axis[0].size;
      size_t sy = data_[0]->axis[1].size;
      if (vs == 1)
      {
         unsigned char max = 0;
         unsigned char *ptr = (unsigned char *)(data_[0]->data);
         for (int i=0; i<nx_; i++)
            for (int j=0; j<ny_; j++)
               for (int k=0; k<nz_; k++)
               {
                  long long index = (long long)(sx)* (long long)(sy) *
                     (long long)(oz_+k) + (long long)(sx) *
                     (long long)(oy_+j) + (long long)(ox_+i);
                  max = ptr[index]>max?ptr[index]:max;
               }
         if (max == 0)
            priority_ = 1;
         else
            priority_ = 0;
      }
      else if (vs == 2)
      {
         unsigned short max = 0;
         unsigned short *ptr = (unsigned short *)(data_[0]->data);
         for (int i=0; i<nx_; i++)
            for (int j=0; j<ny_; j++)
               for (int k=0; k<nz_; k++)
               {
                  long long index = (long long)(sx)* (long long)(sy) *
                     (long long)(oz_+k) + (long long)(sx) *
                     (long long)(oy_+j) + (long long)(ox_+i);
                  max = ptr[index]>max?ptr[index]:max;
               }
         if (max == 0)
            priority_ = 1;
         else
            priority_ = 0;
	  }
   }

   
   void TextureBrick::set_priority_brk(ifstream* ifs, int filetype)
   {
/*	   if (!data_[0])
      {
         priority_ = 0;
         return;
      }
      
	  size_t vs = tex_type_size(tex_type(0));
      if (vs == 1)
      {
         unsigned char max = 0;
         unsigned char *ptr = NULL;
		 if (brkdata_) ptr = (unsigned char *)(brkdata_);
		 else {
			 ptr = new unsigned char[nx_*ny_*nz_];
			 if (!read_brick((char *)ptr, nx_*ny_*nz_*vs, ifs, filetype))
			 {
				 delete [] ptr;
				 priority_ = 0;
				 return;
			 }
		 }
         for (int i=0; i<nx_; i++)
            for (int j=0; j<ny_; j++)
               for (int k=0; k<nz_; k++)
               {
                  long long index = (long long)(k) * (long long)(ny_) * (long long)(nx_) +
									(long long)(j) * (long long)(nx_) + 
									(long long)(i);
                  max = ptr[index]>max?ptr[index]:max;
               }
         if (max == 0)
            priority_ = 1;
         else
            priority_ = 0;

		 if (!data_[0]->data) delete [] ptr;
      }
      else if (vs == 2)
      {
         unsigned short max = 0;
         unsigned short *ptr;
		 if (brkdata_) ptr = (unsigned short *)(brkdata_);
		 else {
			 ptr = new unsigned short[nx_*ny_*nz_];
			 if (!read_brick((char *)ptr, nx_*ny_*nz_*vs, ifs, filetype))
			 {
				 delete [] ptr;
				 priority_ = 0;
				 return;
			 }
		 }
         for (int i=0; i<nx_; i++)
            for (int j=0; j<ny_; j++)
               for (int k=0; k<nz_; k++)
               {
                  long long index = (long long)(k) * (long long)(ny_) * (long long)(nx_) +
									(long long)(j) * (long long)(nx_) + 
									(long long)(i);
                  max = ptr[index]>max?ptr[index]:max;
               }
         if (max == 0)
            priority_ = 1;
         else
            priority_ = 0;
		 
		 if (!brkdata_) delete [] ptr;
      }
*/   }

   bool TextureBrick::read_brick(char* data, size_t size, std::wstring* fname, int filetype, bool useURL)
   {
	   if (!fname) return false;
	   
	   if (useURL)
	   {
		   if (filetype == BRICK_FILE_TYPE_RAW)  return raw_brick_reader_url(data, size, fname);
		   if (filetype == BRICK_FILE_TYPE_JPEG) return jpeg_brick_reader_url(data, size, fname);
	   }
	   else
	   {
		   if (filetype == BRICK_FILE_TYPE_RAW)  return raw_brick_reader(data, size, fname);
		   if (filetype == BRICK_FILE_TYPE_JPEG) return jpeg_brick_reader(data, size, fname);
	   }

	   return false;
   }

   bool TextureBrick::raw_brick_reader(char* data, size_t size, std::wstring* fname)
   {
	   try
	   {
		   ifstream ifs;
		   ifs.open(*fname, ios::binary);
		   if (!ifs) return false;
		   ifs.read(data, size);
		   if (ifs) ifs.close();
/*		   
		   ofstream ofs1;
		   wstring str = *fname + wstring(L".txt");
		   ofs1.open(str);
		   for(int i=0; i < size/2; i++){
			   ofs1 << ((unsigned short *)data)[i] << "\n";
		   }
		   ofs1.close();
*/		   
	   }
	   catch (std::exception &e)
	   {
		   cerr << typeid(e).name() << "\n" << e.what() << endl;
		   return false;
	   }

	   return true;
   }

   #define DOWNLOAD_BUFSIZE 8192

   
   struct MemoryStruct {
	   char *memory;
	   size_t size;
   };
   
   static size_t
	   WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
   {
	   size_t realsize = size * nmemb;
	   struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	   mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
	   if(mem->memory == NULL) {
		   /* out of memory! */ 
		   printf("not enough memory (realloc returned NULL)\n");
		   return 0;
	   }

	   memcpy(&(mem->memory[mem->size]), contents, realsize);
	   mem->size += realsize;
	   mem->memory[mem->size] = 0;

	   return realsize;
   }


   bool TextureBrick::raw_brick_reader_url(char* data, size_t size, std::wstring* fname)
   {
	   /*
	   wxURL url(wxString::wxString(*fname));
	   if(url.GetError() != wxURL_NOERR) return false;
	   wxInputStream *in = url.GetInputStream();
	   if(!in || !in->IsOk()) return false;
	   unsigned char buffer[DOWNLOAD_BUFSIZE];
	   size_t count = -1;
	   wxMemoryBuffer mem_buf;
	   char *cur = data;
	   size_t total = 0;
	   while(!in->Eof() && count != 0 && total < size)
	   {
		   in->Read(buffer, DOWNLOAD_BUFSIZE-1);
		   count = in->LastRead();
		   if(count > 0){
			   memcpy(cur, buffer, count);
			   cur += count;
			   total += count;
		   }
	   }
	   if(total != size)return false;
	   */
	   extern CURL *_g_curl;
	   CURLcode ret;
	   struct MemoryStruct chunk;
	   chunk.memory = (char *)malloc(1);  /* will be grown as needed by the realloc above */ 
	   chunk.size = 0;

	   if (_g_curl == NULL) {
		   cerr << "curl_easy_init() failed" << endl;
		   return false;
	   }
	   curl_easy_reset(_g_curl);
	   curl_easy_setopt(_g_curl, CURLOPT_URL, wxString::wxString(*fname).ToStdString().c_str());
	   curl_easy_setopt(_g_curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	   curl_easy_setopt(_g_curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	   curl_easy_setopt(_g_curl, CURLOPT_WRITEDATA, &chunk);
	   //ピア証明書検証なし
	   curl_easy_setopt(_g_curl, CURLOPT_SSL_VERIFYPEER, 0);
	   ret = curl_easy_perform(_g_curl);
	   if (ret != CURLE_OK) {
		   cerr << "curl_easy_perform() failed." << curl_easy_strerror(ret) << endl;
		   return false;
	   }
	   if(chunk.size != size) return false;

	   memcpy(data, chunk.memory, size);

	   if(chunk.memory)
		   free(chunk.memory);

	   return true;
   }

   struct my_error_mgr {
	   struct jpeg_error_mgr pub;	/* "public" fields */

	   jmp_buf setjmp_buffer;	/* for return to caller */
   };

   typedef struct my_error_mgr * my_error_ptr;

   METHODDEF(void)
	   my_error_exit (j_common_ptr cinfo)
   {
	   /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	   my_error_ptr myerr = (my_error_ptr) cinfo->err;

	   /* Always display the message. */
	   /* We could postpone this until after returning, if we chose. */
	   (*cinfo->err->output_message) (cinfo);

	   /* Return control to the setjmp point */
	   longjmp(myerr->setjmp_buffer, 1);
   }

   bool TextureBrick::jpeg_brick_reader(char* data, size_t size, std::wstring* fname)
   {
	   jpeg_decompress_struct cinfo;
	   struct my_error_mgr jerr;
	   
	   FILE *fp = _wfopen(fname->c_str(), L"rb");
	   if (!fp) return false;

	   cinfo.err = jpeg_std_error(&jerr.pub);
	   jerr.pub.error_exit = my_error_exit;
	   /* Establish the setjmp return context for my_error_exit to use. */
	   if (setjmp(jerr.setjmp_buffer)) {
		   /* If we get here, the JPEG code has signaled an error.
		   * We need to clean up the JPEG object, close the input file, and return.
		   */
		   jpeg_destroy_decompress(&cinfo);
		   fclose(fp);
		   return false;
	   }
	   jpeg_create_decompress(&cinfo);
	   jpeg_stdio_src(&cinfo, fp);
	   jpeg_read_header(&cinfo, TRUE);
	   jpeg_start_decompress(&cinfo);
	   
	   if(cinfo.jpeg_color_space != JCS_GRAYSCALE || cinfo.output_components != 1) longjmp(jerr.setjmp_buffer, 1);
	   
	   size_t jpgsize = cinfo.output_width * cinfo.output_height;
	   if (jpgsize != size) longjmp(jerr.setjmp_buffer, 1);
	   unsigned char *ptr[1];
	   ptr[0] = (unsigned char *)data;
	   while (cinfo.output_scanline < cinfo.output_height) {
		   ptr[0] = (unsigned char *)data + cinfo.output_scanline * cinfo.output_width;
		   jpeg_read_scanlines(&cinfo, ptr, 1);
	   }

	   jpeg_destroy_decompress(&cinfo);
	   fclose(fp);
	   
	   return true;
   }

   bool TextureBrick::jpeg_brick_reader_url(char* data, size_t size, std::wstring* fname)
   {
	   jpeg_decompress_struct cinfo;
	   struct my_error_mgr jerr;
	   
	   extern CURL *_g_curl;
	   CURLcode ret;
	   struct MemoryStruct chunk;
	   chunk.memory = (char *)malloc(1);
	   chunk.size = 0;

	   if (_g_curl == NULL) {
		   cerr << "curl_easy_init() failed" << endl;
		   return false;
	   }
	   curl_easy_reset(_g_curl);
	   curl_easy_setopt(_g_curl, CURLOPT_URL, wxString::wxString(*fname).ToStdString().c_str());
	   curl_easy_setopt(_g_curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	   curl_easy_setopt(_g_curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	   curl_easy_setopt(_g_curl, CURLOPT_WRITEDATA, &chunk);
	   //ピア証明書検証なし
	   curl_easy_setopt(_g_curl, CURLOPT_SSL_VERIFYPEER, 0);
	   ret = curl_easy_perform(_g_curl);
	   if (ret != CURLE_OK) {
		   cerr << "curl_easy_perform() failed." << curl_easy_strerror(ret) << endl;
		   return false;
	   }
	   
	   /*
	   wxURL url(wxString::wxString(*fname));
	   if(url.GetError() != wxURL_NOERR) return false;
	   wxInputStream *in = url.GetInputStream();
	   if(!in || !in->IsOk()) return false;
	   unsigned char buffer[DOWNLOAD_BUFSIZE];
	   size_t count = -1;
	   wxMemoryBuffer mem_buf;
	   while(!in->Eof() && count != 0)
	   {
		   in->Read(buffer, DOWNLOAD_BUFSIZE-1);
		   count = in->LastRead();
		   if(count > 0) mem_buf.AppendData(buffer, count);
	   }
	   */

	   cinfo.err = jpeg_std_error(&jerr.pub);
	   jerr.pub.error_exit = my_error_exit;
	   /* Establish the setjmp return context for my_error_exit to use. */
	   if (setjmp(jerr.setjmp_buffer)) {
		   /* If we get here, the JPEG code has signaled an error.
		   * We need to clean up the JPEG object, close the input file, and return.
		   */
		   jpeg_destroy_decompress(&cinfo);
		   return false;
	   }
	   jpeg_create_decompress(&cinfo);
	   jpeg_mem_src(&cinfo, (unsigned char *)chunk.memory, chunk.size);
	   //jpeg_mem_src(&cinfo, (unsigned char *)mem_buf.GetData(), mem_buf.GetDataLen());
	   jpeg_read_header(&cinfo, TRUE);
	   jpeg_start_decompress(&cinfo);
	   
	   if(cinfo.jpeg_color_space != JCS_GRAYSCALE || cinfo.output_components != 1) longjmp(jerr.setjmp_buffer, 1);
	   
	   size_t jpgsize = cinfo.output_width * cinfo.output_height;
	   if (jpgsize != size) longjmp(jerr.setjmp_buffer, 1);
	   unsigned char *ptr[1];
	   ptr[0] = (unsigned char *)data;
	   while (cinfo.output_scanline < cinfo.output_height) {
		   ptr[0] = (unsigned char *)data + cinfo.output_scanline * cinfo.output_width;
		   jpeg_read_scanlines(&cinfo, ptr, 1);
	   }

	   jpeg_destroy_decompress(&cinfo);
	   
	   if(chunk.memory)
		   free(chunk.memory);
	   	   
	   return true;
   }

   void TextureBrick::freeBrkData()
   {
	   if (brkdata_) delete [] brkdata_;
	   brkdata_ = NULL;
   }
} // end namespace FLIVR
