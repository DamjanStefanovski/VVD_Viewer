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

#ifndef SLIVR_Texture_h
#define SLIVR_Texture_h

#include <vector>
#include <fstream>
#include "Transform.h"
#include "TextureBrick.h"
#include "Utils.h"

namespace FLIVR
{
	using namespace std;

	class Transform;

	class Texture 
	{
	public:
		static size_t mask_undo_num_;
		Texture();
		virtual ~Texture();

		bool build(Nrrd* val, Nrrd* grad,
			 double vmn, double vmx,
			 double gmn, double gmx,
			 vector<FLIVR::TextureBrick*>* brks = NULL);

		inline Vector res() { return Vector(nx_, ny_, nz_); }
		inline int nx() { return nx_; }
		inline int ny() { return ny_; }
		inline int nz() { return nz_; }

		inline int nc() { return nc_; }
		inline int nb(int i)
		{
			assert(i >= 0 && i < TEXTURE_MAX_COMPONENTS);
			return nb_[i];
		}
		inline int nmask() { return nmask_; }
		inline int nlabel() { return nlabel_; }

		inline void set_size(int nx, int ny, int nz, int nc, int* nb) 
		{
			nx_ = nx; ny_ = ny; nz_ = nz; nc_ = nc;
			for(int c = 0; c < nc_; c++)
			{
				nb_[c] = nb[c];
			}
			if (nc==1)
			{
				ntype_[0] = TYPE_INT;
			}
			else if (nc>1)
			{
				ntype_[0] = TYPE_INT_GRAD;
				ntype_[1] = TYPE_GM;
			}
		}


		//! Interface that does not expose FLIVR::BBox.
		inline 
			void get_bounds(double &xmin, double &ymin, double &zmin,
			double &xmax, double &ymax, double &zmax) const 
		{
			BBox b;
			get_bounds(b);
			xmin = b.min().x();
			ymin = b.min().y();
			zmin = b.min().z();

			xmax = b.max().x();
			ymax = b.max().y();
			zmax = b.max().z();
		}

		inline 
			void get_bounds(BBox &b) const 
		{
			b.extend(transform_.project(bbox_.min()));
			b.extend(transform_.project(bbox_.max()));
		}

		inline BBox *bbox() { return &bbox_; }
		inline void set_bbox(BBox bbox) { bbox_ = bbox; }
		inline Transform *transform() { return &transform_; }
		inline void set_transform(Transform tform) { transform_ = tform; }

		// get sorted bricks
		vector<TextureBrick*>* get_sorted_bricks(
			Ray& view, bool is_orthographic = false);
		//get closest bricks
		vector<TextureBrick*>* get_closest_bricks(
			Point& center, int quota, bool skip,
			Ray& view, bool is_orthographic = false);
		//set sort bricks
		void set_sort_bricks() {sort_bricks_ = true;}
		void reset_sort_bricks() {sort_bricks_ = false;}
		bool get_sort_bricks() {return sort_bricks_;}
		// load the bricks independent of the view
		vector<TextureBrick*>* get_bricks();
		int get_brick_num() {return int((*bricks_).size());}
		//quota bricks
		vector<TextureBrick*>* get_quota_bricks();

		int get_brick_id_point(int ix, int iy, int iz);
		//relative coordinate
		double get_brick_original_value(int brick_id, int i, int j, int k, bool normalize);
		//absolute coordinate
		double get_brick_original_value(int i, int j, int k, bool normalize);

		inline int nlevels(){ return int((*bricks_).size()); }

		inline double vmin() const { return vmin_; }
		inline double vmax() const { return vmax_; }
		inline double gmin() const { return gmin_; }
		inline double gmax() const { return gmax_; }
		inline void set_minmax(double vmin, double vmax, double gmin, double gmax)
		{vmin_ = vmin; vmax_ = vmax; gmin_ = gmin; gmax_ = gmax;}

		void set_spacings(double x, double y, double z);
		void set_base_spacings(double x, double y, double z);
		void get_spacings(double &x, double &y, double &z, int lv = -1);
		void get_base_spacings(double &x, double &y, double &z);
		void set_spacing_scales(double x, double y, double z);
		void get_spacing_scales(double &x, double &y, double &z);
		
			// Creator of the brick owns the nrrd memory.
		void set_nrrd(Nrrd* data, int index);
		Nrrd* get_nrrd(int index)
		{if (index>=0&&index<TEXTURE_MAX_COMPONENTS) return data_[index]; else return 0;}
		int get_max_tex_comp()
		{return TEXTURE_MAX_COMPONENTS;}
		bool trim_mask_undos_head();
		bool trim_mask_undos_tail();
		bool get_undo();
		bool get_redo();
		void set_mask(void* mask_data);
		void push_mask();
		void mask_undos_forward();
		void mask_undos_backward();
		void clear_undos();

		//add one more texture component as the volume mask
		bool add_empty_mask();
		//add one more texture component as the labeling volume
		bool add_empty_label();

		void delete_mask();
		void delete_label();

		//get priority brick number
		inline void set_use_priority(bool value) {use_priority_ = value;}
		inline bool get_use_priority() {return use_priority_;}
		inline int get_n_p0()
		{if (use_priority_) return n_p0_; else return int((*bricks_).size());}

		//for brkxml file
		void set_data_file(vector<FileLocInfo *> *fname, int type);
		int GetFileType() {return filetype_;}
		FileLocInfo *GetFileName(int id);
		bool isBrxml() {return brkxml_;}
		bool isURL() {return useURL_;}
		bool buildPyramid(vector<Pyramid_Level> &pyramid, vector<vector<vector<vector<FileLocInfo *>>>> &filenames, bool useURL = false);
		void set_FrameAndChannel(int fr, int ch);
		void setLevel(int lv);
		Nrrd * loadData(int &lv);
		int GetCurLevel() {return pyramid_cur_lv_;}
		int GetLevelNum() {return pyramid_.size();}
		void SetCopyableLevel(int lv) {pyramid_copy_lv_ = lv;}
		int GetCopyableLevel() {return pyramid_copy_lv_;}

		void DeleteCacheFiles();

	protected:
		void build_bricks(vector<TextureBrick*> &bricks,
			int nx, int ny, int nz,
			int nc, int* nb);

		//! data carved up to texture memory sized chunks.
		vector<TextureBrick*>						*bricks_;
		//for limited number of bricks during interactions
		vector<TextureBrick*>						quota_bricks_;
		//sort texture brick
		bool sort_bricks_;
		//! data size
		int											nx_;
		int											ny_;
		int											nz_; 
		//! number of components currently used.
		int											nc_; 
		//type of all the components
		enum CompType
		{
			TYPE_NONE=0, TYPE_INT, TYPE_INT_GRAD, TYPE_GM, TYPE_MASK, TYPE_LABEL
		};
		CompType									ntype_[TEXTURE_MAX_COMPONENTS];
		//the index of current mask
		int											nmask_;
		//the index of current label
		int											nlabel_;
		//! bytes per texel for each component.
		int											nb_[TEXTURE_MAX_COMPONENTS];
		//! data tform
		Transform									transform_; 
		double										vmin_;
		double										vmax_;
		double										gmin_;
		double										gmax_;
		//! data bbox
		BBox										bbox_; 
		//! spacings
		double										spcx_;
		double										spcy_;
		double										spcz_;
		//! base spacings (for brxml)
		double										b_spcx_;
		double										b_spcy_;
		double										b_spcz_;
		//! scales of spacings (for brxml)
		double										s_spcx_;
		double										s_spcy_;
		double										s_spcz_;
		//priority
		bool use_priority_;
		int n_p0_;

		//for brkxml
		vector<FileLocInfo *> *filename_;
		int filetype_;
		ifstream ifs_;
		bool brkxml_;
		bool useURL_;

		int pyramid_cur_lv_;
		int pyramid_cur_fr_;
		int pyramid_cur_ch_;
		int pyramid_lv_num_;
		vector<Pyramid_Level> pyramid_;
		vector<vector<vector<vector<FileLocInfo *>>>> filenames_;

		int pyramid_copy_lv_;

		//used when brkxml_ is not equal to false.
		vector<TextureBrick*> default_vec_;

		void clearPyramid();

		Nrrd* data_[TEXTURE_MAX_COMPONENTS];
		//undos for mask
		vector<void*> mask_undos_;
		int mask_undo_pointer_;
	};

} // namespace FLIVR

#endif // Volume_Texture_h