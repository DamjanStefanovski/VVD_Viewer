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

#ifndef TextureRenderer_h
#define TextureRenderer_h

#include <nrrd.h>
#include "TextureBrick.h"
#include "Texture.h"
#include <stdint.h>
#include <glm/glm.hpp>
#include <unordered_set>
#include <unordered_map>
#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>
#include <string>
#include <tinyxml2.h>

namespace FLIVR
{
   //a simple fixed-length fifo sequence
   class BrickQueue
   {
      public:
         BrickQueue(int limit):
            m_queue(0),
            m_limit(limit),
            m_pos(0)
      {
         if (m_limit >=0)
         {
            m_queue = new int[m_limit];
            memset(m_queue, 0, m_limit*sizeof(int));
         }
      }
         ~BrickQueue()
         {
            if (m_queue)
               delete []m_queue;
         }

         int GetLimit()
         {return m_limit;}
         int Push(int value)
         {
            if (m_queue)
            {
               m_queue[m_pos] = value;
               if (m_pos < m_limit-1)
                  m_pos++;
               else
                  m_pos = 0;
               return 1;
            }
            else
               return 0;
         }
         int Get(int index)
         {
            if (index>=0 && index<m_limit)
               return m_queue[m_pos+index<m_limit?m_pos+index:m_pos+index-m_limit];
            else
               return 0;
         }
         int GetLast()
         {
            return m_queue[m_pos==0?m_limit-1:m_pos-1];
         }

      private:
         int *m_queue;
         int m_limit;
         int m_pos;
   };

   class ShaderProgram;
   class VolShaderFactory;
   class SegShaderFactory;
   class VolCalShaderFactory;
   class VolKernelFactory;

   struct TexParam
   {
      int nx, ny, nz, nb;
      unsigned int id;
      TextureBrick *brick;
      int comp;
      GLenum textype;
      bool delayed_del;
      TexParam() :
         nx(0), ny(0), nz(0), nb(0),
         id(0), brick(0), comp(0),
         textype(GL_UNSIGNED_BYTE),
         delayed_del(false)
      {}
      TexParam(int c, int x,
            int y, int z,
            int b, GLenum f,
            unsigned int i) :
          nx(x), ny(y),
         nz(z), nb(b), id(i),
         brick(0), comp(c), textype(f),
         delayed_del(false)
      {}
   };

#define PALETTE_W 256
#define PALETTE_H 256
#define PALETTE_SIZE (PALETTE_W*PALETTE_H)
#define PALETTE_ELEM_COMP 4

   class TextureRenderer
   {
      public:
         enum RenderMode
         {
            MODE_NONE,
            MODE_OVER,
            MODE_MIP,
            MODE_SLICE
         };

         TextureRenderer(Texture* tex);
         TextureRenderer(const TextureRenderer&);
         virtual ~TextureRenderer();

         //set the texture for rendering
         void set_texture(Texture* tex);
		void reset_texture();

         //set blending bits. b>8 means 32bit blending
         void set_blend_num_bits(int b);

         //clear the opengl textures from the texture pool
         static void clear_tex_pool();
		void clear_tex_current();

         //resize the fbo texture
         void resize();

         //set the 2d texture mask for segmentation
         void set_2d_mask(GLuint id);
         //set 2d weight map for segmentation
         void set_2d_weight(GLuint weight1, GLuint weight2);

         //set the 2d texture depth map for rendering shadows
         void set_2d_dmap(GLuint id);

         // Tests the bounding box against the current MODELVIEW and
         // PROJECTION matrices to determine if it is within the viewport.
         // Returns true if it is visible.
		bool test_against_view(const BBox &bbox, bool persp=false);

		 void clear_brick_buf();

		 void init_palette();
		 void update_palette_tex();
		 void set_roi_name(wstring name, int id=-1, wstring parent_name=wstring());
		 void set_roi_name(wstring name, int id, int parent_id);
		 wstring check_new_roi_name(wstring name);
		 int add_roi_group_node(int parent_id, wstring name=L"");
		 int add_roi_group_node(wstring parent_name=L"", wstring name=L"");
		 int get_available_group_id();
		 int get_next_sibling_roi(int id);
		 void move_roi_node(int src_id, int dst_id, int insert_mode=0);
		 bool insert_roi_node(boost::property_tree::wptree& tree, int dst_id, const boost::property_tree::wptree& node, int id, int insert_mode=0);
		 void erase_node(int id=-1);
		 void erase_node(wstring name);
		 wstring get_roi_name(int id=-1);
		 void set_roi_select(wstring name, bool select, bool traverse=false);
		 void set_roi_select_children(wstring name, bool select, bool traverse=false);
		 void set_roi_select_r(const boost::property_tree::wptree& tree, bool select, bool recursive=true);
		 void select_all_roi_tree(){ set_roi_select_r(roi_tree_, true); update_palette(desel_palette_mode_, desel_col_fac_); }
		 void deselect_all_roi_tree(){ set_roi_select_r(roi_tree_, false); update_palette(desel_palette_mode_, desel_col_fac_); }
		 void deselect_all_roi(){ clear_sel_ids_roi_only(); update_palette(desel_palette_mode_, desel_col_fac_); }
		 void update_sel_segs();
		 void update_sel_segs(const boost::property_tree::wptree& tree);
		 boost::property_tree::wptree *get_roi_tree(){ return &roi_tree_; }
		 boost::optional<wstring> get_roi_path(int id);
		 boost::optional<wstring> get_roi_path(int id, const boost::property_tree::wptree& tree, const wstring& parent);
		 boost::optional<wstring> get_roi_path(wstring name);
		 boost::optional<wstring> get_roi_path(wstring name, const boost::property_tree::wptree& tree, const wstring& parent);
		 int get_roi_id(wstring name);
		 void set_id_color(unsigned char r, unsigned char g, unsigned char b, bool update=true, int id=-1);
		 void get_id_color(unsigned char &r, unsigned char &g, unsigned char &b, int id=-1);
		 void get_rendered_id_color(unsigned char &r, unsigned char &g, unsigned char &b, int id=-1);
		 //0-dark; 1-gray; 2-invisible;
		 void update_palette(int mode, float fac=0.1);
		 int get_roi_disp_mode(){ return desel_palette_mode_; }
		 void set_desel_palette_mode_dark(float fac=0.1);
		 void set_desel_palette_mode_gray(float fac=0.1);
		 void set_desel_palette_mode_invisible();
		 GLuint get_palette();
		 bool is_sel_id(int id);
		 void add_sel_id(int id);
		 void del_sel_id(int id);
		 void set_edit_sel_id(int id);
		 int get_edit_sel_id(){ return edit_sel_id_; };
		 void clear_sel_ids();
		 void clear_sel_ids_roi_only();
		 void clear_roi();
		 void import_roi_tree_xml(const wstring &filepath);
		 void import_roi_tree_xml_r(tinyxml2::XMLElement *lvNode, const boost::property_tree::wptree& tree, const wstring& parent, int& gid);
		 wstring export_roi_tree();
		 void export_roi_tree_r(wstring &buf, const boost::property_tree::wptree& tree, const wstring& parent);
		 string exprot_selected_roi_ids();
		 void import_roi_tree(const wstring &tree);
		 void import_selected_ids(const string &sel_ids_str);
		 
         //memory swap
         static void set_mem_swap(bool val) {mem_swap_ = val;}
         static bool get_mem_swap() {return mem_swap_;}
         //use memory limit (otherwise get available memory from graphics card)
         static void set_use_mem_limit(bool val) {use_mem_limit_ = val;}
         static bool get_use_mem_limit() {return use_mem_limit_;}
         //memory limit
         static void set_mem_limit(double val) {mem_limit_ = val;}
         static double get_mem_limit() {return mem_limit_;}
         //available memory
         static void set_available_mem(double val) {available_mem_ = val;}
         static double get_available_mem() {return available_mem_;}
		 //main(cpu) memory limit
         static void set_mainmem_buf_size(double val) {mainmem_buf_size_ = val;}
         static double get_mainmem_buf_size() {return mainmem_buf_size_;}
         //available main(cpu) memory
         static void set_available_mainmem_buf_size(double val) {available_mainmem_buf_size_ = val;}
         static double get_available_mainmem_buf_size() {return available_mainmem_buf_size_;}
         //large data size
         static void set_large_data_size(double val) {large_data_size_ = val;}
         static double get_large_data_size() {return large_data_size_;}
         //force brick size
         static void set_force_brick_size(int val) {force_brick_size_ = val;}
         static int get_force_brick_size() {return force_brick_size_;}
         //callback checks if the swapping is done
         static void set_update_loop() {start_update_loop_ = true;
            done_update_loop_ = false;}
            static void reset_update_loop() {start_update_loop_ = false;}
            static bool get_done_update_loop() {return done_update_loop_;}
            static void set_done_update_loop(bool b) {done_update_loop_ = b;}
            static bool get_start_update_loop() {return start_update_loop_;}
            static bool get_done_current_chan() {return done_current_chan_;}
            static void reset_done_current_chan()
            {done_current_chan_ = false; cur_chan_brick_num_ = 0;
               save_final_buffer_ = true; clear_chan_buffer_ = true;}
               static int get_cur_chan_brick_num() {return cur_chan_brick_num_;}
               static int get_cur_brick_num() {return cur_brick_num_;}
			   static void set_total_brick_num(int num) {total_brick_num_ = num; cur_brick_num_ = 0; cur_tid_offset_multi_ = 0;}
               static int get_total_brick_num() {return total_brick_num_;}
               static void reset_clear_chan_buffer() {clear_chan_buffer_ = false;}
               static bool get_clear_chan_buffer() {return clear_chan_buffer_;}
               static void set_clear_chan_buffer(bool b) {clear_chan_buffer_ = b;}
               static void reset_save_final_buffer() {save_final_buffer_ = false;}
               static bool get_save_final_buffer() {return save_final_buffer_;}
			   static void set_save_final_buffer() {save_final_buffer_ = true;}
               //set start time
               static void set_st_time(unsigned long time) {st_time_ = time;}
               static unsigned long get_st_time() {return st_time_;}
               static void set_up_time(unsigned long time) {up_time_ = time;}
               static unsigned long get_up_time();
               static void set_consumed_time(unsigned long time) {consumed_time_ = time;}
               static unsigned long get_consumed_time() {return consumed_time_;}
               //set corrected up time according to mouse speed
               static void set_cor_up_time(int speed);
               static unsigned long get_cor_up_time() {return cor_up_time_;}
               //interactive mdoe
               static void set_interactive(bool mode) {interactive_ = mode;}
               static bool get_interactive() {return interactive_;}
               //number of bricks rendered before time is up
               static void reset_finished_bricks();
               static int get_finished_bricks() {return finished_bricks_;}
               static void set_finished_bricks(int i) {finished_bricks_ = i;}
               static int get_finished_bricks_max();
               static int get_est_bricks(int mode);
               static int get_queue_last() {return brick_queue_.GetLast();}
               //quota bricks in interactive mode
               static void set_quota_bricks(int quota) {quota_bricks_ = quota;}
               static int get_quota_bricks() {return quota_bricks_;}
               //current channel
               void set_quota_bricks_chan(int quota) {quota_bricks_chan_ = quota;}
               int get_quota_bricks_chan() {return quota_bricks_chan_;}
               //quota center
               static void set_qutoa_center(Point &point) {quota_center_ = point;}
               static Point get_quota_center() {return quota_center_; }
               //update order
               static void set_update_order(int val) {update_order_ = val;}
               static int get_update_order() {return update_order_;}

			   static void set_load_on_main_thread(bool val) {load_on_main_thread_ = val;}
			   static bool get_load_on_main_thread() {return load_on_main_thread_;}

			   static VolKernelFactory vol_kernel_factory_;

      public:
               struct BrickDist
               {
                  unsigned int index;    //index of the brick in current tex pool
                  TextureBrick* brick;  //a brick
                  double dist;      //distance to another brick
               };
               Texture *tex_;
               RenderMode mode_;
               double sampling_rate_;
               int num_slices_;
               double irate_;
               bool imode_;

               //blend frame buffer for output
               bool blend_framebuffer_resize_;
               GLuint blend_framebuffer_;
               GLuint blend_tex_id_;
			   GLuint label_tex_id_;
               //2nd buffer for multiple filtering
               bool filter_buffer_resize_;
               GLuint filter_buffer_;
               GLuint filter_tex_id_;

			   GLuint palette_tex_id_;
			   GLuint base_palette_tex_id_;
			   unsigned char palette_[PALETTE_SIZE*PALETTE_ELEM_COMP];
			   unsigned char base_palette_[PALETTE_SIZE*PALETTE_ELEM_COMP];
			   unordered_set<int> sel_ids_;
			   unordered_set<int> sel_segs_;
			   boost::property_tree::wptree roi_tree_;
			   int desel_palette_mode_;
			   float desel_col_fac_;
			   int edit_sel_id_;

               //sahder for volume rendering
               static VolShaderFactory vol_shader_factory_;
               //shader for segmentation
               static SegShaderFactory seg_shader_factory_;
               //shader for calculation
               static VolCalShaderFactory cal_shader_factory_;

               //3d frame buffer object for mask
               GLuint fbo_mask_;
               //3d frame buffer object for label
               GLuint fbo_label_;
               //2d mask texture
               GLuint tex_2d_mask_;
               //2d weight map
               GLuint tex_2d_weight1_;  //after tone mapping
               GLuint tex_2d_weight2_;  //before tone mapping
               //2d depth map texture
               GLuint tex_2d_dmap_;

               int blend_num_bits_;
               static bool clear_pool_;

			   struct LoadedBrick {
				   bool swapped;
				   TextureBrick *brk;
				   double size;
			   };
			   static vector<LoadedBrick> loadedbrks;
			   static int del_id;
			   
               //memory management
               static bool mem_swap_;
               static bool use_mem_limit_;
               static double mem_limit_;
               static double available_mem_;
			   static double mainmem_buf_size_;
			   static double available_mainmem_buf_size_;
               static double large_data_size_;
               static int force_brick_size_;
               static vector<TexParam> tex_pool_;
               static bool start_update_loop_;
               static bool done_update_loop_;
               static bool done_current_chan_;
               static int total_brick_num_;
               static int cur_brick_num_;
               static int cur_chan_brick_num_;
               static bool clear_chan_buffer_;
               static bool save_final_buffer_;
			   static int cur_tid_offset_multi_;
               //timer for rendering
               static unsigned long st_time_;
               static unsigned long up_time_;
               static unsigned long cor_up_time_;//mouse movement corrected up time
               static unsigned long consumed_time_;
               //interactive mode
               static bool interactive_;
               //number of rendered blocks before time is up
               static int finished_bricks_;
               static BrickQueue brick_queue_;
               //quota in interactive mode
               static int quota_bricks_;
               int quota_bricks_chan_;//for current channel
               //center point of the quota
               static Point quota_center_;
               //update order
               static int update_order_;

			   static bool load_on_main_thread_;

               //for view testing
               float mvmat_[16];
               float prmat_[16];

			   //opengl matrices
			   glm::mat4 m_mv_mat;
			   glm::mat4 m_mv_mat2;
			   glm::mat4 m_proj_mat;
			   glm::mat4 m_tex_mat;

			   //vertex and index buffer bind points
			   GLuint m_slices_vbo, m_slices_ibo, m_slices_vao;
			   GLuint m_quad_vbo, m_quad_vao;

               //compute view
               Ray compute_view();
			   Ray compute_snapview(double snap);
		double compute_rate_scale(Vector v);

               //brick distance sort
               static bool brick_sort(const BrickDist& bd1, const BrickDist& bd2);
               //check and swap memory
               void check_swap_memory(TextureBrick* brick, int c);
               //load texture bricks for drawing
               //unit:assigned unit, c:channel
               GLint load_brick(int unit, int c, vector<TextureBrick*> *b, int i, GLint filter=GL_LINEAR, bool compression=false, int mode=0, bool set_drawn=true);
               //load the texture for volume mask into texture pool
               GLint load_brick_mask(vector<TextureBrick*> *b, int i, GLint filter=GL_NEAREST, bool compression=false, int unit=0);
               //load the texture for volume labeling into texture pool
               GLint load_brick_label(vector<TextureBrick*> *b, int i);
               void release_texture(int unit, GLenum target);

               //draw slices of the volume
               void draw_slices(double d);

			   void draw_view_quad(double d=0.0);

               //slices
               void draw_polygons(vector<double>& vertex, vector<double>& texcoord,
                     vector<int>& poly,
                     bool fog,
                     ShaderProgram *shader = 0);
               void draw_polygons_wireframe(vector<double>& vertex, vector<double>& texcoord,
                     vector<int>& poly,
                     bool fog);

			   void draw_polygons(vector<float>& vertex,
				   vector<uint32_t>& triangle_verts);
			   void draw_polygons(vector<float>& vertex,
				   vector<uint32_t>& triangle_verts, vector<uint32_t>& size);
			   void draw_polygons_wireframe(vector<float>& vertex,
				   vector<uint32_t>& index, vector<uint32_t>& size);

               //bind 2d mask for segmentation
               void bind_2d_mask();
               //bind 2d weight map for segmentation
               void bind_2d_weight();
               //bind 2d depth map for rendering shadows
               void bind_2d_dmap();

			   void rearrangeLoadedBrkVec();
   };
} // end namespace FLIVR

#endif // TextureRenderer_h