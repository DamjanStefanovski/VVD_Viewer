﻿#include "VRenderView.h"
#include "VRenderFrame.h"
#include "bitmap_fonts.h"
#include <tiffio.h>
#include <wx/utils.h>
#include <wx/valnum.h>
#include <algorithm>
#include <map>
#include <wx/gdicmn.h>
#include <wx/display.h>
#include <wx/stdpaths.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

int VRenderView::m_id = 1;
ImgShaderFactory VRenderGLView::m_img_shader_factory;
PaintShaderFactory VRenderGLView::m_paint_shader_factory;
//budget_test
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
//wxFileOutputStream fos("budget_test.txt");
//wxTextOutputStream tos(fos);

////////////////////////////////////////////////////////////////////////


BEGIN_EVENT_TABLE(LMTreeCtrl, wxTreeCtrl)
	//EVT_SET_FOCUS(LMTreeCtrl::OnSetFocus)
	EVT_MENU(ID_Expand, LMTreeCtrl::OnExpand)
	EVT_TREE_ITEM_ACTIVATED(wxID_ANY, LMTreeCtrl::OnAct)
	EVT_TREE_SEL_CHANGED(wxID_ANY, LMTreeCtrl::OnSelectChanged)
	EVT_KEY_DOWN(LMTreeCtrl::OnKeyDown)
	EVT_KEY_UP(LMTreeCtrl::OnKeyUp)
END_EVENT_TABLE()

LMTreeCtrl::LMTreeCtrl(
      VRenderGLView *glview,
      wxWindow* parent,
      wxWindowID id,
      const wxPoint& pos,
      const wxSize& size,
      long style) :
   wxTreeCtrl(parent, id, pos, size, style),
   m_glview(glview),
   m_schtxtctrl(NULL)
{
	wxTreeItemId rootitem = AddRoot(wxT("Root"));
	LMItemInfo* item_data = new LMItemInfo;
	item_data->type = 0;//root;
	SetItemData(rootitem, item_data);

//	ShowOnlyMachedItems(wxT(""));
}

LMTreeCtrl::~LMTreeCtrl()
{
}

void LMTreeCtrl::Append(wxString name, Point p)
{
}

void LMTreeCtrl::ShowOnlyMachedItems(wxString str)
{
	if (!m_glview) return;

	str.MakeLower();

	UnselectAll();

	SetEvtHandlerEnabled(false);
	Freeze();
	DeleteAllItems();

	// rebuild tree
	wxTreeItemId rootitem = AddRoot(wxT("Root"));
	LMItemInfo* item_data = new LMItemInfo;
	item_data->type = 0;//root;
	SetItemData(rootitem, item_data);

	vector<Ruler*>*  landmarks = m_glview->GetLandmarks();
	for(int i = 0; i < (*landmarks).size(); i++)
	{
		if((*landmarks)[i])
		{
			wxString text = (*landmarks)[i]->GetNameDisp();
			wxString lower = text;
			lower.MakeLower();
			if(str.IsEmpty() || lower.Find(str) == 0)
			{
				wxTreeItemId item = AppendItem(GetRootItem(), text);
				LMItemInfo* item_data = new LMItemInfo;
				item_data->type = 1;
				item_data->p = *(*landmarks)[i]->GetPoint(0);
				SetItemData(item, item_data);
			}
		}
	}
	
	SortChildren(GetRootItem());

	Thaw();
	SetEvtHandlerEnabled(true);

	ExpandAll();
	wxTreeItemIdValue cookie2;
	wxTreeItemId topitem = GetFirstChild(GetRootItem(), cookie2);
	ScrollTo(topitem);
}

void LMTreeCtrl::CopyToSchTextCtrl()
{
	wxTreeItemId sel_item = GetSelection();
	if(sel_item.IsOk() && m_schtxtctrl)
	{
		m_schtxtctrl->SetValue(GetItemText(sel_item));
	}
}

void LMTreeCtrl::StartManip(wxString str)
{
	
}

void LMTreeCtrl::OnSetFocus(wxFocusEvent& event)
{
	CopyToSchTextCtrl();
	Hide();
	event.Skip();
}

void LMTreeCtrl::OnAct(wxTreeEvent &event)
{
	wxTreeItemId sel_item = GetSelection();
	if(sel_item.IsOk())
	{

	}
}

void LMTreeCtrl::OnSelectChanged(wxTreeEvent &event)
{
	wxTreeItemId sel_item = GetSelection();
	if(sel_item.IsOk() && FindFocus() == (wxWindow *)this)
	{
		if (m_glview)
		{
			double cx, cy, cz;
			LMItemInfo* itemdata = (LMItemInfo *)GetItemData(sel_item);
			if (itemdata)
			{
				Point p = itemdata->p;
				m_glview->GetObjCenters(cx, cy, cz);
				m_glview->StartManipulation(NULL, NULL, NULL, &Point(cx-p.x(), -(cy-p.y()), -(cz-p.z())), NULL);
			}
		}
		CopyToSchTextCtrl();
		Hide();
		//Unselect();
	}
	event.Skip();
}

void LMTreeCtrl::OnExpand(wxCommandEvent &event)
{

}

void LMTreeCtrl::OnKeyDown(wxKeyEvent& event)
{
	event.Skip();
}

void LMTreeCtrl::OnKeyUp(wxKeyEvent& event)
{
	event.Skip();
}


////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(LMSeacher, wxTextCtrl)

//EVT_CHILD_FOCUS(LMSeacher::OnSetChildFocus)
EVT_SET_FOCUS(LMSeacher::OnSetFocus)
EVT_KILL_FOCUS(LMSeacher::OnKillFocus)
EVT_TEXT(wxID_ANY, LMSeacher::OnSearchText)
EVT_TEXT_ENTER(wxID_ANY, LMSeacher::OnEnterInSearcher)
//EVT_IDLE(LMSeacher::OnIdle)
EVT_KEY_DOWN(LMSeacher::OnKeyDown)
EVT_KEY_UP(LMSeacher::OnKeyUp)
EVT_MOUSE_EVENTS(LMSeacher::OnMouse)
END_EVENT_TABLE()

LMSeacher::LMSeacher(
			VRenderGLView* glview,
	        wxWindow* parent,
            wxWindowID id,
			const wxString& text,
            const wxPoint& pos,
            const wxSize& size,
			long style) :
		wxTextCtrl(parent, id, "", pos, size, style),
		m_glview(glview)
{
	SetHint(text);

	m_dummy = new wxButton(parent, id, text, pos, size);
	m_dummy->Hide();

	wxSize listsize = size;
	listsize.SetHeight(300);
	m_list = new LMTreeCtrl(glview, parent, id, wxPoint(pos.x, pos.y + 30), listsize);
	m_list->SetSearcher(this);
	
	m_list->Hide();

}

LMSeacher::~LMSeacher()
{

}

void LMSeacher::KillFocus()
{
	if(m_glview) m_glview->SetKeyLock(false);
	m_dummy->SetFocus();
	m_list->Hide();
}

void LMSeacher::OnSetChildFocus(wxChildFocusEvent& event)
{
	if(m_glview) m_glview->SetKeyLock(true);
	m_list->ShowOnlyMachedItems(GetValue());
	m_list->Show();
	event.Skip();
}

void LMSeacher::OnSetFocus(wxFocusEvent& event)
{
	if(m_glview) m_glview->SetKeyLock(true);
	m_list->ShowOnlyMachedItems(GetValue());
	m_list->Show();
	event.Skip();
}

void LMSeacher::OnKillFocus(wxFocusEvent& event)
{
	if(m_glview) m_glview->SetKeyLock(false);
	m_list->Hide();
	event.Skip();
}

void LMSeacher::OnSearchText(wxCommandEvent &event)
{
	if(m_glview) m_glview->SetKeyLock(true);
	m_list->ShowOnlyMachedItems(GetValue());
	m_list->Show();
}

void LMSeacher::OnEnterInSearcher(wxCommandEvent &event)
{
	if(m_glview) m_glview->SetKeyLock(false);
	m_list->CopyToSchTextCtrl();
	wxString str = GetValue();
	str.MakeLower();
	if (m_glview)
	{
		vector<Ruler*>*  landmarks = m_glview->GetLandmarks();
		for(int i = 0; i < (*landmarks).size(); i++)
		{
			if((*landmarks)[i])
			{
				wxString text = (*landmarks)[i]->GetNameDisp();
				wxString lower = text;
				lower.MakeLower();
				if(lower == str)
				{
					double cx, cy, cz;
					Point *p = (*landmarks)[i]->GetPoint(0);
					m_glview->GetObjCenters(cx, cy, cz);
					m_glview->StartManipulation(NULL, NULL, NULL, &Point(cx-p->x(), -(cy-p->y()), -(cz-p->z())), NULL);
					break;
				}
			}
		}
	}
	//

	m_dummy->SetFocus();
	m_list->Hide();
	event.Skip();
}

void LMSeacher::OnKeyDown(wxKeyEvent& event)
{
	if(wxGetKeyState(WXK_DOWN))
	{
		wxTreeItemId sel_item = m_list->GetSelection();
		if(sel_item.IsOk())
		{
			wxTreeItemId nextsib = m_list->GetNextSibling(sel_item);
			if(nextsib.IsOk()) m_list->SelectItem(nextsib);
		}
		else 
		{
			wxTreeItemIdValue cookie;
			wxTreeItemId first = m_list->GetFirstChild(m_list->GetRootItem(), cookie);
			if(first.IsOk()) m_list->SelectItem(first);
		}
		
		return;
	}
	if(wxGetKeyState(WXK_UP))
	{
		wxTreeItemId sel_item = m_list->GetSelection();
		if(sel_item.IsOk())
		{
			wxTreeItemId prevsib = m_list->GetPrevSibling(sel_item);
			if(prevsib.IsOk()) m_list->SelectItem(prevsib);
			else m_list->UnselectAll();
		}

		return;
	}
	event.Skip();
}

void LMSeacher::OnIdle(wxIdleEvent &event)
{
	if(wxGetKeyState(WXK_DOWN) && m_list->IsShown())
	{
		wxTreeItemId sel_item = m_list->GetSelection();
		if(sel_item.IsOk())
		{
			wxTreeItemId nextsib = m_list->GetNextSibling(sel_item);
			if(nextsib.IsOk()) m_list->SelectItem(nextsib);
		}
		else 
		{
			wxTreeItemIdValue cookie;
			wxTreeItemId first = m_list->GetFirstChild(m_list->GetRootItem(), cookie);
			if(first.IsOk())
			{
				m_list->SelectItem(first);
			}
		}
	}
}

void LMSeacher::OnKeyUp(wxKeyEvent& event)
{
	event.Skip();
}

 void LMSeacher::OnMouse(wxMouseEvent& event)
 {
	 event.Skip();
 }

 /////////////////////////////////////////////////////////////////////////


BEGIN_EVENT_TABLE(VRenderGLView, wxGLCanvas)
EVT_PAINT(VRenderGLView::OnDraw)
EVT_SIZE(VRenderGLView::OnResize)
EVT_MOUSE_EVENTS(VRenderGLView::OnMouse)
EVT_IDLE(VRenderGLView::OnIdle)
EVT_KEY_DOWN(VRenderGLView::OnKeyDown)
END_EVENT_TABLE()

VRenderGLView::VRenderGLView(wxWindow* frame,
      wxWindow* parent,
      wxWindowID id,
      wxGLContext* sharedContext,
      int * attribList,
      const wxPoint& pos,
      const wxSize& size,
      long style) :
   wxGLCanvas(parent, id, attribList, pos, size, style),
   //public
   //capture modes
   m_capture(false),
   m_capture_rotat(false),
   m_capture_rotate_over(false),
   m_capture_tsequ(false),
   m_capture_bat(false),
   m_capture_param(false),
   //begin and end frame
   m_begin_frame(0),
   m_end_frame(0),
   //counters
   m_tseq_cur_num(0),
   m_bat_cur_num(0),
   m_param_cur_num(0),
   m_total_frames(0),
   //hud
   m_updating(false),
   m_draw_annotations(true),
   m_draw_camctr(false),
   m_camctr_size(2.0),
   m_draw_info(false),
   m_draw_coord(false),
   m_drawing_coord(false),
   m_draw_frame(false),
   m_test_speed(false),
   m_draw_clip(false),
   m_draw_legend(false),
   m_mouse_focus(false),
   m_test_wiref(false),
   m_draw_rulers(true),
   //current volume
   m_cur_vol(0),
   //clipping settings
   m_clip_mask(-1),
   m_clip_mode(2),
   //scale bar
   m_disp_scale_bar(false),
   m_disp_scale_bar_text(false),
   m_sb_length(50),
   m_sb_unit(1),
   m_sb_height(0.0),
   //ortho size
   m_ortho_left(0.0),
   m_ortho_right(0.0),
   m_ortho_bottom(0.0),
   m_ortho_top(0.0),
   //scale factor
   m_scale_factor(1.0),
   m_scale_factor_saved(1.0),
   //mode in determining depth of volume
   m_point_volume_mode(0),
   //ruler use volume transfer function
   m_ruler_use_transf(false),
   //ruler time dependent
   m_ruler_time_dep(true),
   //linked rotation
   m_linked_rot(false),
   //private
   m_frame(frame),
   m_vrv((VRenderView*)parent),
   //populated lists of data
   m_vd_pop_dirty(true),
   m_md_pop_dirty(true),
   //ruler type
   m_ruler_type(0),
   m_editing_ruler_point(0),
   //traces
   m_trace_group(0),
   //multivolume
   m_mvr(0),
   //initializaion
   m_initialized(false),
   m_init_view(false),
   //bg color
   m_bg_color(0.0, 0.0, 0.0),
   m_bg_color_inv(1.0, 1.0, 1.0),
   m_grad_bg(false),
   //frustrum
   m_aov(15.0),
   m_near_clip(0.1),
   m_far_clip(100.0),
   //interpolation
   m_intp(true),
   //previous focus
   m_prev_focus(0),
   //interactive modes
   m_int_mode(1),
   m_force_clear(false),
   m_interactive(false),
   m_adaptive(true),
   m_brush_state(0),
   //resizing
   m_resize(false),
   m_resize_ol1(false),
   m_resize_ol2(false),
   //brush tools
   m_draw_brush(false),
   m_paint_enable(false),
   m_paint_display(false),
   //2d frame buffers
   m_fbo(0),
   m_tex(0),
   m_tex_wt2(0),
   m_fbo_final(0),
   m_tex_final(0),
   //temp buffer for large data comp
   m_fbo_temp(0),
   m_tex_temp(0),
   //shading (shadow) overlay
   m_fbo_ol1(0),
   m_fbo_ol2(0),
   m_tex_ol1(0),
   m_tex_ol2(0),
   //paint buffer
   m_fbo_paint(0),
   m_tex_paint(0),
   m_clear_paint(true),
   //pick buffer
   m_fbo_pick(0),
   m_tex_pick(0),
   m_tex_pick_depth(0),
   //camera controls
   m_persp(false),
   m_free(false),
   //camera distance
   m_distance(10.0),
   m_init_dist(10.0),
   //camera translation
   m_transx(0.0), m_transy(0.0), m_transz(0.0),
   m_transx_saved(0.0), m_transy_saved(0.0), m_transz_saved(0.0),
   //camera rotation
   m_rotx(0.0), m_roty(0.0), m_rotz(0.0),
   m_rotx_saved(0.0), m_roty_saved(0.0), m_rotz_saved(0.0),
   //camera center
   m_ctrx(0.0), m_ctry(0.0), m_ctrz(0.0),
   m_ctrx_saved(0.0), m_ctry_saved(0.0), m_ctrz_saved(0.0),
   //camera direction
   m_up(0.0, 1.0, 0.0),
   m_head(0.0, 0.0, -1.0),
   //object center
   m_obj_ctrx(0.0), m_obj_ctry(0.0), m_obj_ctrz(0.0),
   //object translation
   m_obj_transx(0.0), m_obj_transy(0.0), m_obj_transz(0.0),
   m_obj_transx_saved(0.0), m_obj_transy_saved(0.0), m_obj_transz_saved(0.0),
   //object rotation
   m_obj_rotx(0.0), m_obj_roty(0.0), m_obj_rotz(0.0),
   m_rot_lock(false),
   //object bounding box
   m_radius(5.0),
   //mouse position
   old_mouse_X(-1), old_mouse_Y(-1),
   prv_mouse_X(-1), prv_mouse_Y(-1),
   //draw controls
   m_draw_all(true),
   m_draw_bounds(false),
   m_draw_grid(false),
   m_draw_type(1),
   m_vol_method(VOL_METHOD_SEQ),
   m_peeling_layers(1),
   m_blend_slices(false),
   //fog
   m_use_fog(true),
   m_fog_intensity(0.0),
   m_fog_start(0.0),
   m_fog_end(0.0),
   //movie properties
   m_init_angle(0.0),
   m_start_angle(0.0),
   m_end_angle(0.0),
   m_cur_angle(0.0),
   m_step(0.0),
   m_rot_axis(0),
   m_movie_seq(0),
   m_rewind(false),
   m_fr_length(0),
   m_stages(0),
   m_4d_rewind(false),
   //movie frame properties
   m_frame_x(-1),
   m_frame_y(-1),
   m_frame_w(-1),
   m_frame_h(-1),
   //post image processing
   m_gamma(Color(1.0, 1.0, 1.0)),
   m_brightness(Color(1.0, 1.0, 1.0)),
   m_hdr(0.0, 0.0, 0.0),
   m_sync_r(false),
   m_sync_g(false),
   m_sync_b(false),
   //volume color map
   m_value_1(0.0),
   m_color_1(Color(0.0, 0.0, 1.0)),
   m_value_2(0.0),
   m_color_2(Color(0.0, 0.0, 1.0)),
   m_value_3(0.25),
   m_color_3(Color(0.0, 1.0, 1.0)),
   m_value_4(0.5),
   m_color_4(Color(0.0, 1.0, 0.0)),
   m_value_5(0.75),
   m_color_5(Color(1.0, 1.0, 0.0)),
   m_value_6(1.0),
   m_color_6(Color(1.0, 0.0, 0.0)),
   m_value_7(1.0),
   m_color_7(Color(1.0, 0.0, 0.0)),
   //paint brush presssure
   m_use_pres(false),
   m_on_press(false),
#ifdef _WIN32
   m_hTab(0),
#endif
   //paint stroke radius
   m_brush_radius1(10),
   m_brush_radius2(30),
   m_use_brush_radius2(true),
   //paint stroke spacing
   m_brush_spacing(0.1),
   //clipping plane rotations
   m_rotx_cl(0), m_roty_cl(0), m_rotz_cl(0),
   m_pressure(0.0),
   //selection
   m_pick(false),
   m_draw_mask(true),
   m_draw_label(false),
   //move view
   m_move_left(false),
   m_move_right(false),
   m_move_up(false),
   m_move_down(false),
   //move time
   m_tseq_forward(false),
   m_tseq_backward(false),
   //move clip
   m_clip_up(false),
   m_clip_down(false),
   m_pre_draw(true),
   m_key_lock(false),
   m_manip(false),
   m_manip_fps(20.0),
   m_manip_time(300),
   m_min_ppi(20),
   m_draw_landmarks(false),
   m_draw_overlays_only(false),
   m_enhance_sel(false),
   m_adaptive_res(false),
   m_int_res(false)
{
   //create context
   if (sharedContext)
   {
      m_glRC = sharedContext;
      m_sharedRC = true;
   }
   else
   {
      m_glRC = new wxGLContext(this);
      m_sharedRC = false;
   }

   goTimer = new nv::Timer(10);
   m_sb_num = "50";
#ifdef _WIN32
   //tablet initialization
   if (m_use_pres && LoadWintab())
   {
      gpWTInfoA(0, 0, NULL);
      m_hTab = TabletInit((HWND)GetHWND());
   }
#endif
   LoadDefaultBrushSettings();

   m_searcher = new LMSeacher(this, (wxWindow *)this, ID_Searcher, wxT("Search"), wxPoint(20, 20), wxSize(200, -1), wxTE_PROCESS_ENTER);
   m_searcher->Hide();
}

void VRenderGLView::SetSearcherVisibility(bool visibility)
{
	if(visibility)
	{
		m_draw_landmarks = true;
		m_searcher->Show();
	}
	else
	{
		m_draw_landmarks = false;
		m_searcher->Hide();
	}
}

#ifdef _WIN32
//tablet init
HCTX VRenderGLView::TabletInit(HWND hWnd)
{
   /* get default region */
   gpWTInfoA(WTI_DEFCONTEXT, 0, &m_lc);

   /* modify the digitizing region */
   m_lc.lcOptions |= CXO_MESSAGES;
   m_lc.lcPktData = PACKETDATA;
   m_lc.lcPktMode = PACKETMODE;
   m_lc.lcMoveMask = PACKETDATA;
   m_lc.lcBtnUpMask = m_lc.lcBtnDnMask;

   /* output in 10000 x 10000 grid */
   m_lc.lcOutOrgX = m_lc.lcOutOrgY = 0;
   m_lc.lcOutExtX = 10000;
   m_lc.lcOutExtY = 10000;

   /* open the region */
   return gpWTOpenA(hWnd, &m_lc, TRUE);

}
#endif

VRenderGLView::~VRenderGLView()
{
   goTimer->stop();
   delete goTimer;
#ifdef _WIN32
   //tablet
   if (m_hTab)
      gpWTClose(m_hTab);
#endif
   int i;
   //delete groups
   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      if (m_layer_list[i]->IsA() == 5)//group
      {
         DataGroup* group = (DataGroup*)m_layer_list[i];
         delete group;
      }
      else if (m_layer_list[i]->IsA() == 6)//mesh group
      {
         MeshGroup* group = (MeshGroup*)m_layer_list[i];
         delete group;
      }
   }

   //delete rulers
   for (i=0; i<(int)m_ruler_list.size(); i++)
   {
      if (m_ruler_list[i])
         delete m_ruler_list[i];
   }

   for(int i = 0; i < m_landmarks.size(); i++)
   {
	   if (m_landmarks[i])
		   delete m_landmarks[i];
   }
   
   //delete buffers and textures
   if (glIsFramebuffer)
   {
      if (glIsFramebuffer(m_fbo))
         glDeleteFramebuffers(1, &m_fbo);
      if (glIsFramebuffer(m_fbo_final))
         glDeleteFramebuffers(1, &m_fbo_final);
      if (glIsFramebuffer(m_fbo_temp))
         glDeleteFramebuffers(1, &m_fbo_temp);
      if (glIsFramebuffer(m_fbo_paint))
         glDeleteFramebuffers(1, &m_fbo_paint);
      if (glIsFramebuffer(m_fbo_ol1))
         glDeleteFramebuffers(1, &m_fbo_ol1);
      if (glIsFramebuffer(m_fbo_ol2))
         glDeleteFramebuffers(1, &m_fbo_ol2);
   }
   if (glIsTexture(m_tex))
      glDeleteTextures(1, &m_tex);
   if (glIsTexture(m_tex_final))
      glDeleteTextures(1, &m_tex_final);
   if (glIsTexture(m_tex_temp))
      glDeleteTextures(1, &m_tex_temp);
   if (glIsTexture(m_tex_paint))
      glDeleteTextures(1, &m_tex_paint);
   if (glIsTexture(m_tex_ol1))
      glDeleteTextures(1, &m_tex_ol1);
   if (glIsTexture(m_tex_ol2))
      glDeleteTextures(1, &m_tex_ol2);

   if (glIsBuffer(m_quad_vbo))
		glDeleteBuffers(1, &m_quad_vbo);
	if (glIsVertexArray(m_quad_vao))
		glDeleteVertexArrays(1, &m_quad_vao);
	if (glIsBuffer(m_misc_vbo))
		glDeleteBuffers(1, &m_misc_vbo);
	if (glIsBuffer(m_misc_ibo))
		glDeleteBuffers(1, &m_misc_ibo);
	if (glIsVertexArray(m_misc_vao))
		glDeleteVertexArrays(1, &m_misc_vao);

	//pick buffer
	if (glIsFramebuffer(m_fbo_pick))
		glDeleteFramebuffers(1, &m_fbo_pick);
	if (glIsTexture(m_tex_pick))
		glDeleteTextures(1, &m_tex_pick);
	if (glIsTexture(m_tex_pick_depth))
		glDeleteTextures(1, &m_tex_pick_depth);

   if (!m_sharedRC)
      delete m_glRC;
}

void VRenderGLView::OnResize(wxSizeEvent& event)
{
   int i;
   for (i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      VolumeData* vd = m_vd_pop_list[i];
      if (vd)
      {
         VolumeRenderer* vr = vd->GetVR();
         if (vr)
            vr->resize();
      }
   }

   if (m_mvr)
      m_mvr->resize();

   m_resize = true;
   m_resize_ol1 = true;
   m_resize_ol2 = true;

   RefreshGL();
}

void VRenderGLView::Init()
{
   if (!m_initialized)
   {
      SetCurrent(*m_glRC);
      ShaderProgram::init_shaders_supported();

      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame) vr_frame->SetTextureRendererSettings();

      glViewport(0, 0, (GLint)(GetSize().x), (GLint)(GetSize().y));
	  goTimer->start();
	  glGenBuffers(1, &m_quad_vbo);
	  m_quad_vao = 0;
	  glGenBuffers(1, &m_misc_vbo);
	  glGenBuffers(1, &m_misc_ibo);
	  glGenVertexArrays(1, &m_misc_vao);
	  glEnable( GL_MULTISAMPLE );
	  m_initialized = true;
   }
}

void VRenderGLView::Clear()
{
   //delete groups
   for (int i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      if (m_layer_list[i]->IsA() == 5)//group
      {
         DataGroup* group = (DataGroup*)m_layer_list[i];
         delete group;
      }
      else if (m_layer_list[i]->IsA() == 6)//mesh group
      {
         MeshGroup* group = (MeshGroup*)m_layer_list[i];
         delete group;
      }
   }

   m_layer_list.clear();
}

void VRenderGLView::HandleProjection(int nx, int ny, bool restrict)
{
   double aspect = (double)nx / (double)ny;
	if (!m_free)
		m_distance = m_radius/tan(d2r(m_aov/2.0))/m_scale_factor;
	if (aspect>1.0)
	{
		m_ortho_left = -m_radius*aspect/m_scale_factor;
		m_ortho_right = -m_ortho_left;
		m_ortho_bottom = -m_radius/m_scale_factor;
		m_ortho_top = -m_ortho_bottom;
	}
	else
	{
		m_ortho_left = -m_radius/m_scale_factor;
		m_ortho_right = -m_ortho_left;
		m_ortho_bottom = -m_radius/aspect/m_scale_factor;
		m_ortho_top = -m_ortho_bottom;
	}
	if (m_persp)
	{
		m_proj_mat = glm::perspective(m_aov, aspect, m_near_clip, m_far_clip);
	}
	else
	{
		m_proj_mat = glm::ortho(m_ortho_left, m_ortho_right, m_ortho_bottom, m_ortho_top,
			-m_far_clip/100.0, m_far_clip);
	}
}

void VRenderGLView::HandleCamera()
{
   Vector pos(m_transx, m_transy, m_transz);
	pos.normalize();
	if (m_free)
		pos *= 0.1;
	else
		pos *= m_distance;
	m_transx = pos.x();
	m_transy = pos.y();
	m_transz = pos.z();

	if (m_free)
		m_mv_mat = glm::lookAt(glm::vec3(m_transx+m_ctrx, m_transy+m_ctry, m_transz+m_ctrz),
		glm::vec3(m_ctrx, m_ctry, m_ctrz),
		glm::vec3(m_up.x(), m_up.y(), m_up.z()));
	else
		m_mv_mat = glm::lookAt(glm::vec3(m_transx, m_transy, m_transz),
		glm::vec3(0.0), glm::vec3(m_up.x(), m_up.y(), m_up.z()));
}

//depth buffer calculation
double VRenderGLView::CalcZ(double z)
{
	double result = 0.0;
	if (m_persp)
	{
		if (z != 0.0)
		{
			result = (m_far_clip+m_near_clip)/(m_far_clip-m_near_clip)/2.0 +
				(-m_far_clip*m_near_clip)/(m_far_clip-m_near_clip)/z+0.5;
		}
	}
	else
		result = (z-m_near_clip)/(m_far_clip-m_near_clip);
	return result;
}

void VRenderGLView::CalcFogRange()
{
	int w, h;
	w = GetSize().x;
	h = GetSize().y;

	BBox bbox;
	bool use_box = false;
	if (m_cur_vol)
	{
		bbox = m_cur_vol->GetBounds();
		use_box = true;
	}
	else if (!m_md_pop_list.empty())
	{
		for (size_t i=0; i<m_md_pop_list.size(); ++i)
		{
			if (m_md_pop_list[i]->GetDisp())
			{
				bbox.extend(m_md_pop_list[i]->GetBounds());
				use_box = true;
			}
		}
	}

	if (use_box)
	{
		Transform mv;
		mv.set(glm::value_ptr(m_mv_mat));

		double minz, maxz;
		minz = numeric_limits<double>::max();
		maxz = -numeric_limits<double>::max();

		vector<Point> points;
		points.push_back(Point(bbox.min().x(), bbox.min().y(), bbox.min().z()));
		points.push_back(Point(bbox.min().x(), bbox.min().y(), bbox.max().z()));
		points.push_back(Point(bbox.min().x(), bbox.max().y(), bbox.min().z()));
		points.push_back(Point(bbox.min().x(), bbox.max().y(), bbox.max().z()));
		points.push_back(Point(bbox.max().x(), bbox.min().y(), bbox.min().z()));
		points.push_back(Point(bbox.max().x(), bbox.min().y(), bbox.max().z()));
		points.push_back(Point(bbox.max().x(), bbox.max().y(), bbox.min().z()));
		points.push_back(Point(bbox.max().x(), bbox.max().y(), bbox.max().z()));

		Point p;
		for (size_t i=0; i<points.size(); ++i)
		{
			p = mv.transform(points[i]);
			minz = p.z()<minz?p.z():minz;
			maxz = p.z()>maxz?p.z():maxz;
		}

		minz = fabs(minz);
		maxz = fabs(maxz);
		m_fog_start = minz<maxz?minz:maxz;
		m_fog_end = maxz>minz?maxz:minz;
	}
	else
	{
		m_fog_start = m_distance - m_radius/2.0;
		m_fog_start = m_fog_start<0.0?0.0:m_fog_start;
		m_fog_end = m_distance + m_radius/4.0;
	}
}

//draw the volume data only
void VRenderGLView::Draw()
{
   int nx = GetSize().x;
   int ny = GetSize().y;

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   // clear color and depth buffers
   glDrawBuffer(GL_BACK);
   glClearDepth(1.0);
   glClearColor(m_bg_color.r(), m_bg_color.g(), m_bg_color.b(), 0.0);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glViewport(0, 0, (GLint)nx, (GLint)ny);

   //gradient background
   if (m_grad_bg)
      DrawGradBg();

   if (VolumeRenderer::get_done_update_loop() && !m_draw_overlays_only) StartLoopUpdate();

   if (m_draw_all)
   {
	   //projection
	   HandleProjection(nx, ny);
	   //Transformation
	   HandleCamera();

	   glm::mat4 mv_temp = m_mv_mat;
	   //translate object
	   m_mv_mat = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	   //rotate object
	   m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	   m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	   m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	   //center object
	   m_mv_mat = glm::translate(m_mv_mat, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

	   if (VolumeRenderer::get_done_update_loop() && m_draw_overlays_only) DrawFinalBuffer();
	   else
	   {
		   if (m_use_fog)
		   {
			   glEnable(GL_FOG);
			   GLfloat FogCol[3]={0.0, 0.0, 0.0};
			   glFogfv(GL_FOG_COLOR, FogCol);
			   glFogi(GL_FOG_MODE, GL_LINEAR);
			   double fog_start = m_distance - m_radius/2.0;
			   fog_start = fog_start<0.0?0.0:fog_start;
			   double fog_end = m_distance + m_radius/4.0;
			   glFogf(GL_FOG_START, GLfloat(fog_start));
			   glFogf(GL_FOG_END, GLfloat(fog_end));
			   glFogf(GL_FOG_DENSITY, GLfloat(m_fog_intensity));

			   CalcFogRange();
		   }

		   if (m_draw_clip)
			   DrawClippingPlanes(false, BACK_FACE);

		   //setup
		   glEnable(GL_BLEND);
		   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		   //draw the volumes
		   DrawVolumes();
	   }


      //draw the clipping planes
      if (m_draw_clip)
         DrawClippingPlanes(true, FRONT_FACE);

      if (m_use_fog)
         glDisable(GL_FOG);

      if (m_draw_bounds)
         DrawBounds();

      if (m_draw_grid)
         DrawGrid();

      if (m_draw_annotations)
      {
         glColor3d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b());
         DrawAnnotations(m_persp);
      }

      if (m_draw_rulers)
      {
         glColor3d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b());
         DrawRulers();
      }

      //traces
      DrawTraces();

	  m_mv_mat = mv_temp;
   }

   m_draw_overlays_only = false;
}

//draw with depth peeling
void VRenderGLView::DrawDP()
{
   int i;
   int nx = GetSize().x;
   int ny = GetSize().y;

   //clear
   glDrawBuffer(GL_BACK);
   glClearDepth(1.0);
   glClearColor(m_bg_color.r(), m_bg_color.g(), m_bg_color.b(), 0.0);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glViewport(0, 0, (GLint)nx, (GLint)ny);

   //gradient background
   if (m_grad_bg)
      DrawGradBg();

   if (VolumeRenderer::get_done_update_loop() && !m_draw_overlays_only) StartLoopUpdate();

   if (m_draw_all)
   {
	   //projection
	   HandleProjection(nx, ny);
	   //Transformation
	   HandleCamera();

	   glm::mat4 mv_temp = m_mv_mat;
	   //translate object
	   m_mv_mat = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	   //rotate object
	   m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	   m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	   m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	   //center object
	   m_mv_mat = glm::translate(m_mv_mat, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

	   if (VolumeRenderer::get_done_update_loop() && m_draw_overlays_only) DrawFinalBuffer();
	   else
	   {
		   if (m_use_fog)
		   {
			   glEnable(GL_FOG);
			   float FogCol[3]={0.0, 0.0, 0.0};
			   glFogfv(GL_FOG_COLOR, FogCol);
			   glFogi(GL_FOG_MODE, GL_LINEAR);
			   float fog_start = m_distance - m_radius/2.0;
			   fog_start = fog_start<0.0?0.0:fog_start;
			   float fog_end = m_distance + m_radius/4.0;
			   glFogf(GL_FOG_START, fog_start);
			   glFogf(GL_FOG_END, fog_end);
			   glFogf(GL_FOG_DENSITY, m_fog_intensity);

			   CalcFogRange();
		   }

		   if (m_draw_clip)
			   DrawClippingPlanes(true, BACK_FACE);

		   //generate depth peeling buffers
		   for (i=0; i<m_peeling_layers; i++)
		   {
			   if (i>=(int)m_dp_fbo_list.size())
			   {
				   GLuint fbo_id;
				   GLuint tex_id;
				   glGenFramebuffers(1, &fbo_id);
				   glGenTextures(1, &tex_id);
				   glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
				   glBindTexture(GL_TEXTURE_2D, tex_id);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
				   glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, nx, ny, 0,
					   GL_DEPTH_COMPONENT, GL_FLOAT, NULL);//GL_RGBA16F
				   glFramebufferTexture2D(GL_FRAMEBUFFER,
					   GL_DEPTH_ATTACHMENT,
					   GL_TEXTURE_2D, tex_id, 0);
				   m_dp_fbo_list.push_back(fbo_id);
				   m_dp_tex_list.push_back(tex_id);
			   }
			   else if (i>=0 && !glIsFramebuffer(m_dp_fbo_list[i]))
			   {
				   GLuint fbo_id;
				   GLuint tex_id;
				   glGenFramebuffers(1, &fbo_id);
				   glGenTextures(1, &tex_id);
				   glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
				   glBindTexture(GL_TEXTURE_2D, tex_id);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
				   glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, nx, ny, 0,
					   GL_DEPTH_COMPONENT, GL_FLOAT, NULL);//GL_RGBA16F
				   glFramebufferTexture2D(GL_FRAMEBUFFER,
					   GL_DEPTH_ATTACHMENT,
					   GL_TEXTURE_2D, tex_id, 0);
				   m_dp_fbo_list[i] = fbo_id;
				   m_dp_tex_list[i] = tex_id;
			   }
			   if (m_resize)
			   {
				   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[i]);
				   glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, nx, ny, 0,
					   GL_DEPTH_COMPONENT, GL_FLOAT, NULL);//GL_RGBA16F
				   //m_resize = false;
			   }
		   }

		   //setup
		   glDisable(GL_LIGHTING);
		   glDisable(GL_BLEND);
		   glEnable(GL_DEPTH_TEST);
		   glDepthFunc(GL_LEQUAL);
		   glDisable(GL_FOG);

		   //draw depth values of each layer into the buffers
		   for (i=0; i<m_peeling_layers; i++)
		   {
			   glBindFramebuffer(GL_FRAMEBUFFER, m_dp_fbo_list[i]);
			   glDrawBuffer(GL_NONE);
			   glReadBuffer(GL_NONE);

			   glClearDepth(1.0);
			   glClear(GL_DEPTH_BUFFER_BIT);

			   if (i==0)
			   {
				   DrawMeshes(0);
			   }
			   else
			   {
				   glActiveTexture(GL_TEXTURE15);
				   glEnable(GL_TEXTURE_2D);
				   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[i-1]);
				   glActiveTexture(GL_TEXTURE0);
				   DrawMeshes(1);
				   glActiveTexture(GL_TEXTURE15);
				   glDisable(GL_TEXTURE_2D);
				   glBindTexture(GL_TEXTURE_2D, 0);
				   glActiveTexture(GL_TEXTURE0);
			   }
		   }

		   //bind back the framebuffer
		   glBindFramebuffer(GL_FRAMEBUFFER, 0);

		   //setup
		   if (m_use_fog)
			   glEnable(GL_FOG);

		   //draw depth peeling
		   for (i=m_peeling_layers; i>=0; i--)
		   {
			   if (i==0)
			   {
				   //draw volumes before the depth
				   glActiveTexture(GL_TEXTURE15);
				   glEnable(GL_TEXTURE_2D);
				   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[0]);
				   glActiveTexture(GL_TEXTURE0);
				   DrawVolumes(1);
				   glActiveTexture(GL_TEXTURE15);
				   glDisable(GL_TEXTURE_2D);
				   glBindTexture(GL_TEXTURE_2D, 0);
				   glActiveTexture(GL_TEXTURE0);
			   }
			   else
			   {
				   if (m_peeling_layers == 1)
				   {
					   //i == m_peeling_layers == 1
					   glActiveTexture(GL_TEXTURE15);
					   glEnable(GL_TEXTURE_2D);
					   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[0]);
					   glActiveTexture(GL_TEXTURE0);
				   }
				   else if (m_peeling_layers == 2)
				   {
					   glActiveTexture(GL_TEXTURE14);
					   glEnable(GL_TEXTURE_2D);
					   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[0]);
					   glActiveTexture(GL_TEXTURE15);
					   glEnable(GL_TEXTURE_2D);
					   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[1]);
					   glActiveTexture(GL_TEXTURE0);
				   }
				   else if (m_peeling_layers > 2)
				   {
					   if (i == m_peeling_layers)
					   {
						   glActiveTexture(GL_TEXTURE14);
						   glEnable(GL_TEXTURE_2D);
						   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[i-2]);
						   glActiveTexture(GL_TEXTURE15);
						   glEnable(GL_TEXTURE_2D);
						   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[i-1]);
						   glActiveTexture(GL_TEXTURE0);
					   }
					   else if (i == 1)
					   {
						   glActiveTexture(GL_TEXTURE14);
						   glEnable(GL_TEXTURE_2D);
						   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[0]);
						   glActiveTexture(GL_TEXTURE15);
						   glEnable(GL_TEXTURE_2D);
						   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[1]);
						   glActiveTexture(GL_TEXTURE0);
					   }
					   else
					   {
						   glActiveTexture(GL_TEXTURE13);
						   glEnable(GL_TEXTURE_2D);
						   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[i-2]);
						   glActiveTexture(GL_TEXTURE14);
						   glEnable(GL_TEXTURE_2D);
						   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[i-1]);
						   glActiveTexture(GL_TEXTURE15);
						   glEnable(GL_TEXTURE_2D);
						   glBindTexture(GL_TEXTURE_2D, m_dp_tex_list[i]);
						   glActiveTexture(GL_TEXTURE0);
					   }
				   }

				   //draw volumes
				   if (m_peeling_layers == 1)
					   //i == m_peeling_layers == 1
						   DrawVolumes(5);//draw volume after 15
				   else if (m_peeling_layers == 2)
				   {
					   if (i == 2)
						   DrawVolumes(2);//draw volume after 15
					   else if (i == 1)
						   DrawVolumes(4);//draw volume after 14 and before 15
				   }
				   else if (m_peeling_layers > 2)
				   {
					   if (i == m_peeling_layers)
						   DrawVolumes(2);//draw volume after 15
					   else if (i == 1)
						   DrawVolumes(4);//draw volume after 14 and before 15
					   else
						   DrawVolumes(3);//draw volume after 14 and before 15
				   }

				   //draw meshes
				   glEnable(GL_DEPTH_TEST);
				   glDepthFunc(GL_LEQUAL);
				   glDisable(GL_LIGHTING);
				   glEnable(GL_BLEND);
				   glBlendEquation(GL_FUNC_ADD);
				   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				   if (m_use_fog)
					   glEnable(GL_FOG);

				   if (m_peeling_layers == 1)
					   //i == m_peeling_layers == 1
						   DrawMeshes(5);//draw mesh at 15
				   else if (m_peeling_layers == 2)
				   {
					   if (i == 2)
						   DrawMeshes(2);//draw mesh after 14
					   else if (i == 1)
						   DrawMeshes(4);//draw mesh before 15
				   }
				   else if (m_peeling_layers > 2)
				   {
					   if (i == m_peeling_layers)
						   DrawMeshes(2);//draw mesh after 14
					   else if (i == 1)
						   DrawMeshes(4);//draw mesh before 15
					   else
						   DrawMeshes(3);//draw mesh after 13 and before 15
				   }

				   glActiveTexture(GL_TEXTURE13);
				   glDisable(GL_TEXTURE_2D);
				   glBindTexture(GL_TEXTURE_2D, 0);
				   glActiveTexture(GL_TEXTURE14);
				   glDisable(GL_TEXTURE_2D);
				   glBindTexture(GL_TEXTURE_2D, 0);
				   glActiveTexture(GL_TEXTURE15);
				   glDisable(GL_TEXTURE_2D);
				   glBindTexture(GL_TEXTURE_2D, 0);
				   glActiveTexture(GL_TEXTURE0);

			   }
		   }

		   if (m_dp_tex_list.size()>0)
		   {
			   double darkness;
			   if (GetMeshShadow(darkness))
				   DrawOLShadowsMesh(m_dp_tex_list[0], darkness);
		   }

		   if (m_use_fog)
			   glDisable(GL_FOG);
	   }

      if (m_draw_clip)
         DrawClippingPlanes(false, FRONT_FACE);

      if (m_draw_bounds)
         DrawBounds();

      if (m_draw_grid)
         DrawGrid();

      if (m_draw_annotations)
      {
         glColor3d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b());
         DrawAnnotations(m_persp);
      }

	  m_mv_mat = mv_temp;

   }
}

//draw meshes
//peel==true -- depth peeling
void VRenderGLView::DrawMeshes(int peel)
{
   for (int i=0 ; i<(int)m_layer_list.size() ; i++)
   {
      if (!m_layer_list[i])
         continue;
      if (m_layer_list[i]->IsA() == 3)
      {
         MeshData* md = (MeshData*)m_layer_list[i];
         if (md && md->GetDisp())
		 {
			 md->SetMatrices(m_mv_mat, m_proj_mat);
			 md->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
			 md->Draw(peel);
		 }
      }
      else if (m_layer_list[i]->IsA() == 6)
      {
         MeshGroup* group = (MeshGroup*)m_layer_list[i];
         if (group && group->GetDisp())
         {
            for (int j=0; j<(int)group->GetMeshNum(); j++)
            {
               MeshData* md = group->GetMeshData(j);
               if (md && md->GetDisp())
			   {
				   md->SetMatrices(m_mv_mat, m_proj_mat);
				   md->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
				   md->Draw(peel);
			   }
            }
         }
      }
   }
}

//draw volumes
//peel==true -- depth peeling
void VRenderGLView::DrawVolumes(int peel)
{
   int finished_bricks = 0;
   if (TextureRenderer::get_mem_swap())
   {
      finished_bricks = TextureRenderer::get_finished_bricks();
      TextureRenderer::reset_finished_bricks();
   }

   PrepFinalBuffer();

   //draw
   if (m_updating ||
         (!m_drawing_coord &&
          (m_int_mode == 1 ||
           m_int_mode == 3 ||
           m_int_mode == 4 ||
           m_int_mode == 5 ||
           (m_int_mode == 6 &&
            !m_editing_ruler_point) ||
           m_int_mode == 8 ||
           m_force_clear)))
   {
      m_updating = false;
      m_force_clear = false;

      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame &&
            vr_frame->GetSettingDlg() &&
            vr_frame->GetSettingDlg()->GetUpdateOrder() == 1)
      {
         //if (m_interactive) // deleted by takashi
            ClearFinalBuffer();
      }
      else
         ClearFinalBuffer();

      GLboolean bCull = glIsEnabled(GL_CULL_FACE);
      glDisable(GL_CULL_FACE);

      PopVolumeList();

      vector<VolumeData*> quota_vd_list;
      if (TextureRenderer::get_mem_swap())
      {
         //set start time for the texture renderer
         TextureRenderer::set_st_time(GET_TICK_COUNT());
		 
         TextureRenderer::set_interactive(m_interactive);
         //if in interactive mode, do interactive bricking also
         if (m_interactive && !(m_vol_method == VOL_METHOD_MULTI))
         {
            //calculate quota
            int total_bricks = TextureRenderer::get_total_brick_num();
            int quota_bricks = 0;
            if (finished_bricks < total_bricks)
               quota_bricks = TextureRenderer::get_est_bricks(3);
            else
               quota_bricks = total_bricks;
            quota_bricks = Min(total_bricks, int(double(quota_bricks) *
                     double(TextureRenderer::get_cor_up_time()) /
                     double(TextureRenderer::get_up_time())));
            TextureRenderer::set_quota_bricks(quota_bricks);
            int quota_bricks_chan = 0;
            if (m_vd_pop_list.size() > 1)
            {
               //priority: 1-selected channel; 2-group contains selected channel; 3-linear distance to above
               //not considering mask for now
               vector<VolumeData*>::iterator cur_iter;
               cur_iter = find(m_vd_pop_list.begin(), m_vd_pop_list.end(), m_cur_vol);
               size_t cur_index = distance(m_vd_pop_list.begin(), cur_iter);
               int vd_index;
               if (cur_iter != m_vd_pop_list.end())
               {
                  VolumeData* vd;
                  vd = *cur_iter;
                  quota_vd_list.push_back(vd);
                  int count_bricks = vd->GetBrickNum();
                  quota_bricks_chan = Min(count_bricks, quota_bricks);
                  vd->GetVR()->set_quota_bricks_chan(quota_bricks_chan);
                  int count = 0;
                  while (count_bricks < quota_bricks &&
                        quota_vd_list.size() < m_vd_pop_list.size())
                  {
                     if (count % 2 == 0)
                        vd_index = cur_index + count/2 + 1;
                     else
                        vd_index = cur_index - count/2 - 1;
                     count++;
                     if (vd_index<0 ||
                           vd_index>=m_vd_pop_list.size())
                        continue;
                     vd = m_vd_pop_list[vd_index];
                     int brick_num = vd->GetBrickNum();
                     quota_vd_list.push_back(vd);
                     if (count_bricks+brick_num > quota_bricks)
                        quota_bricks_chan = quota_bricks - count_bricks;
                     else
                        quota_bricks_chan = brick_num;
                     vd->GetVR()->set_quota_bricks_chan(quota_bricks_chan);
                     count_bricks += quota_bricks_chan;
                  }
               }
            }
            else if (m_vd_pop_list.size() == 1)
            {
               quota_bricks_chan = quota_bricks;
               VolumeData* vd = m_vd_pop_list[0];
               if (vd)
                  vd->GetVR()->set_quota_bricks_chan(quota_bricks_chan);
            }

            //get and set center point
            VolumeData* vd = m_cur_vol;
            if (!vd)
               if (m_vd_pop_list.size())
                  vd = m_vd_pop_list[0];
            Point p;
            if (vd && (GetPointVolumeBox(p,
                        GetSize().x/2, GetSize().y/2,
                        vd, false)>0.0 ||
                     GetPointPlane(p, GetSize().x/2,
                        GetSize().y/2, false)>0.0))
            {
               int resx, resy, resz;
               double sclx, scly, sclz;
               double spcx, spcy, spcz;
               vd->GetResolution(resx, resy, resz);
               vd->GetScalings(sclx, scly, sclz);
               vd->GetSpacings(spcx, spcy, spcz);
               p = Point(p.x()/(resx*sclx*spcx),
                     p.y()/(resy*scly*spcy),
                     p.z()/(resz*sclz*spcz));
               TextureRenderer::set_qutoa_center(p);
            }
            else
               TextureRenderer::set_interactive(false);
         }
      }

	  //takashi_debug
	  //ofstream ofs;
	  //double mvmat[16];
	  //ofs.open("matDrawVolumes.txt");
	  //glGetDoublev(GL_MODELVIEW_MATRIX, mvmat);
	  //for(int n = 0; n < 16; n++)ofs << mvmat[n] << endl;
	  //ofs.close();

      //DrawVolumesComp(m_vd_pop_list, peel);
      if (m_vol_method == VOL_METHOD_MULTI)
      {
/*         if (TextureRenderer::get_mem_swap() &&
               TextureRenderer::get_interactive() &&
               quota_vd_list.size()>0)
            DrawVolumesMulti(quota_vd_list, peel);
         else
*/            DrawVolumesMulti(m_vd_pop_list, peel);
         //draw masks
         if (m_draw_mask)
            DrawVolumesComp(m_vd_pop_list, peel, true);
      }
      else
      {
         int i, j;
         vector<VolumeData*> list;
         for (i=(int)m_layer_list.size()-1; i>=0; i--)
         {
            if (!m_layer_list[i])
               continue;
            switch (m_layer_list[i]->IsA())
            {
            case 2://volume data (this won't happen now)
               {
                  VolumeData* vd = (VolumeData*)m_layer_list[i];
                  if (vd && vd->GetDisp())
                  {
                     if (TextureRenderer::get_mem_swap() &&
                           TextureRenderer::get_interactive() &&
                           quota_vd_list.size()>0)
                     {
                        if (find(quota_vd_list.begin(),
                                 quota_vd_list.end(), vd)!=
                              quota_vd_list.end())
                           list.push_back(vd);
                     }
                     else
                        list.push_back(vd);
                  }
               }
               break;
            case 5://group
               {
                  if (!list.empty())
                  {
                     DrawVolumesComp(list, peel);
                     //draw masks
                     if (m_draw_mask)
                        DrawVolumesComp(list, peel, true);
                     list.clear();
                  }
                  DataGroup* group = (DataGroup*)m_layer_list[i];
                  if (!group->GetDisp())
                     continue;
                  for (j=group->GetVolumeNum()-1; j>=0; j--)
                  {
                     VolumeData* vd = group->GetVolumeData(j);
                     if (vd && vd->GetDisp())
                     {
                        if (TextureRenderer::get_mem_swap() &&
                              TextureRenderer::get_interactive() &&
                              quota_vd_list.size()>0)
                        {
                           if (find(quota_vd_list.begin(),
                                    quota_vd_list.end(), vd)!=
                                 quota_vd_list.end())
                              list.push_back(vd);
                        }
                        else
                           list.push_back(vd);
                     }
                  }
                  if (!list.empty())
                  {
                     if (group->GetBlendMode() == VOL_METHOD_MULTI)
                        DrawVolumesMulti(list, peel);
                     else
                        DrawVolumesComp(list, peel);
                     //draw masks
                     if (m_draw_mask)
                        DrawVolumesComp(list, peel, true);
                     list.clear();
                  }
               }
               break;
            }
         }
      }

      if (bCull) glEnable(GL_CULL_FACE);
   }

   //final composition
   DrawFinalBuffer();

   if (TextureRenderer::get_mem_swap())
   {
      TextureRenderer::set_consumed_time(GET_TICK_COUNT() - TextureRenderer::get_st_time());
      if (TextureRenderer::get_start_update_loop() &&
            TextureRenderer::get_done_update_loop())
         TextureRenderer::reset_update_loop();
   }

   if (m_interactive)
   {
      m_interactive = false;
      RefreshGL();
   }

   if (m_int_res)
   {
	   m_int_res = false;
	   RefreshGL();
   }

   if (m_manip)
   {
	   m_pre_draw = true;
	   RefreshGL();
   }
}

void VRenderGLView::DrawAnnotations(bool persp)
{
   int i;
   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 4://annotation layer
         Annotations* ann = (Annotations*)m_layer_list[i];
         if (ann->GetDisp())
			 ann->Draw(persp, m_mv_mat, m_proj_mat);
         break;
      }
   }
}

//get populated mesh list
//stored in m_md_pop_list
void VRenderGLView::PopMeshList()
{
   if (!m_md_pop_dirty)
      return;

   int i, j;
   m_md_pop_list.clear();

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         return;
      switch (m_layer_list[i]->IsA())
      {
      case 3://mesh data
         {
            MeshData* md = (MeshData*)m_layer_list[i];
            if (md->GetDisp())
               m_md_pop_list.push_back(md);
         }
         break;
      case 6://mesh group
         {
            MeshGroup* group = (MeshGroup*)m_layer_list[i];
            if (!group->GetDisp())
               continue;
            for (j=0; j<group->GetMeshNum(); j++)
            {
               if (group->GetMeshData(j) &&
                     group->GetMeshData(j)->GetDisp())
                  m_md_pop_list.push_back(group->GetMeshData(j));
            }
         }
         break;
      }
   }
   m_md_pop_dirty = false;
}

//get populated volume list
//stored in m_vd_pop_list
void VRenderGLView::PopVolumeList()
{
   if (!m_vd_pop_dirty)
      return;

   int i, j;
   m_vd_pop_list.clear();

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 2://volume data
         {
            VolumeData* vd = (VolumeData*)m_layer_list[i];
            if (vd->GetDisp())
               m_vd_pop_list.push_back(vd);
         }
         break;
      case 5://group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            if (!group->GetDisp())
               continue;
            for (j=0; j<group->GetVolumeNum(); j++)
            {
               if (group->GetVolumeData(j) && group->GetVolumeData(j)->GetDisp())
                  m_vd_pop_list.push_back(group->GetVolumeData(j));
            }
         }
         break;
      }
   }
   m_vd_pop_dirty = false;
}

//organize layers in view
//put all volume data under view into last group of the view
//if no group in view
void VRenderGLView::OrganizeLayers()
{
   DataGroup* le_group = 0;
   int i;

   //find last empty group
   for (i=GetLayerNum()-1; i>=0; i--)
   {
      TreeLayer* layer = GetLayer(i);
      if (layer && layer->IsA()==5)
      {
         //layer is group
         DataGroup* group = (DataGroup*)layer;
         if (group->GetVolumeNum() == 0)
         {
            le_group = group;
            break;
         }
      }
   }

   for (i=0; i<GetLayerNum(); i++)
   {
      TreeLayer* layer = GetLayer(i);
      if (layer && layer->IsA() == 2)
      {
         //layer is volume
         VolumeData* vd = (VolumeData*)layer;
         wxString name = vd->GetName();
         if (le_group)
         {
            RemoveVolumeData(name);
            le_group->InsertVolumeData(le_group->GetVolumeNum(), vd);
         }
         else
         {
            wxString group_name = AddGroup("");
            le_group = GetGroup(group_name);
            if (le_group)
            {
               RemoveVolumeData(name);
               le_group->InsertVolumeData(le_group->GetVolumeNum(), vd);
            }
         }

         VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
         if (vr_frame)
         {
            AdjustView* adjust_view = vr_frame->GetAdjustView();
            if (adjust_view)
            {
               adjust_view->SetGroupLink(le_group);
               adjust_view->UpdateSync();
            }
         }
      }
   }
}

void VRenderGLView::RandomizeColor()
{
   for (int i=0; i<(int)m_layer_list.size(); i++)
   {
      if (m_layer_list[i])
         m_layer_list[i]->RandomizeColor();
   }
}

void VRenderGLView::ClearVolList()
{
   m_vd_pop_list.clear();
}

void VRenderGLView::ClearMeshList()
{
   m_md_pop_list.clear();
}

//interactive modes
int VRenderGLView::GetIntMode()
{
   return m_int_mode;
}

void VRenderGLView::SetIntMode(int mode)
{
   m_int_mode = mode;
   if (m_int_mode == 1)
   {
      m_brush_state = 0;
      m_draw_brush = false;
   }
}

//set use 2d rendering results
void VRenderGLView::SetPaintUse2d(bool use2d)
{
   m_selector.SetPaintUse2d(use2d);
}

bool VRenderGLView::GetPaintUse2d()
{
   return m_selector.GetPaintUse2d();
}

//segmentation mdoe selection
void VRenderGLView::SetPaintMode(int mode)
{
   m_selector.SetMode(mode);
   m_brush_state = mode;
}

int VRenderGLView::GetPaintMode()
{
   return m_selector.GetMode();
}

//draw the brush shape
void VRenderGLView::DrawBrush()
{
   double pressure = m_use_pres&&m_on_press?m_pressure:1.0;

   wxPoint pos1(old_mouse_X, old_mouse_Y);
   wxRect reg = GetClientRect();
   if (reg.Contains(pos1))
   {
      int i;
      int nx, ny;
      nx = GetSize().x;
      ny = GetSize().y;
      double cx = pos1.x;
      double cy = ny - pos1.y;

      //draw the circles
      //set up the matrices
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluOrtho2D(0, (GLint)nx, 0, (GLint)ny);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();

      //attributes
      glPushAttrib(GL_ENABLE_BIT);
      glDisable(GL_LIGHTING);
      glDisable(GL_TEXTURE_2D);
      glDisable(GL_DEPTH_TEST);
      GLfloat line_width;
      glGetFloatv(GL_LINE_WIDTH, &line_width);

      int secs = 60;
      double deg = 0.0;
      int mode = m_selector.GetMode();

      if (mode == 1 ||
            mode == 2)
      {
         //draw circle1
         glColor3d(m_bg_color_inv.r(),
               m_bg_color_inv.g(),
               m_bg_color_inv.b());
         glLineWidth(0.5);
         glBegin(GL_LINE_LOOP);
         for (i=0; i<secs; i++)
         {
            deg = i*2*PI/secs;
            glVertex2f(cx + m_brush_radius1*pressure*sin(deg),
                  cy + m_brush_radius1*pressure*cos(deg));
         }
         glEnd();
      }

      if (mode == 1 ||
            mode == 2 ||
            mode == 3 ||
            mode == 4)
      {
         //draw circle2
         glColor3d(m_bg_color_inv.r(),
               m_bg_color_inv.g(),
               m_bg_color_inv.b());
         glLineWidth(0.5);
         glBegin(GL_LINE_LOOP);
         for (i=0; i<secs; i++)
         {
            deg = i*2*PI/secs;
            glVertex2f(cx + m_brush_radius2*pressure*sin(deg),
                  cy + m_brush_radius2*pressure*cos(deg));
         }
         glEnd();
      }

      char str[2];
      str[1] = 0;
      switch (mode)
      {
      case 1:
         str[0] = 'S';
         beginRenderText(nx, ny);
         renderText(int(cx)-7, ny-int(cy)+3, BITMAP_FONT_TYPE_TIMES_ROMAN_24, str);
         endRenderText();
         break;
      case 2:
         str[0] = '+';
         beginRenderText(nx, ny);
         renderText(int(cx)-7, ny-int(cy)+3, BITMAP_FONT_TYPE_TIMES_ROMAN_24, str);
         endRenderText();
         break;
      case 3:
         str[0] = '-';
         beginRenderText(nx, ny);
         renderText(int(cx)-7, ny-int(cy)+3, BITMAP_FONT_TYPE_TIMES_ROMAN_24, str);
         endRenderText();
         break;
      case 4:
         str[0] = '*';
         beginRenderText(nx, ny);
         renderText(int(cx)-7, ny-int(cy)+3, BITMAP_FONT_TYPE_TIMES_ROMAN_24, str);
         endRenderText();
         break;
      }

      glPopAttrib();
      glLineWidth(line_width);

      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
   }
}

//paint strokes on the paint fbo
void VRenderGLView::PaintStroke()
{
   int nx, ny;
   nx = GetSize().x;
   ny = GetSize().y;

   double pressure = m_use_pres?m_pressure:1.0;

   //generate texture and buffer objects
   glEnable(GL_TEXTURE_2D);
   //painting fbo
   if (!glIsFramebuffer(m_fbo_paint))
      glGenFramebuffers(1, &m_fbo_paint);
   //painting texture
   if (!glIsTexture(m_tex_paint))
      glGenTextures(1, &m_tex_paint);

   //set up the painting fbo
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_paint);
   //color buffer of painting
   glBindTexture(GL_TEXTURE_2D, m_tex_paint);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nx, ny, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glFramebufferTexture2D(GL_FRAMEBUFFER,
         GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, m_tex_paint, 0);
   //clear if asked so
   if (m_clear_paint)
   {
      glClearColor(0.0, 0.0, 0.0, 0.0);
      glClear(GL_COLOR_BUFFER_BIT);
      m_clear_paint = false;
   }
   else
   {
      //paint shader
      ShaderProgram* paint_shader =
         m_paint_shader_factory.shader();
      if (paint_shader)
      {
         if (!paint_shader->valid())
         {
            paint_shader->create();
         }
         paint_shader->bind();
      }

      //paint to texture
      //bind fbo for final composition
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_paint);
      glActiveTexture(GL_TEXTURE0);
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, m_tex_paint);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      glDisable(GL_BLEND);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_LIGHTING);

      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();

      double px = double(old_mouse_X-prv_mouse_X);
      double py = double(old_mouse_Y-prv_mouse_Y);
      double dist = sqrt(px*px+py*py);
      double step = m_brush_radius1*pressure*m_brush_spacing;
      int repeat = int(dist/step+0.5);
      double spx = (double)prv_mouse_X;
      double spy = (double)prv_mouse_Y;
      if (repeat > 0)
      {
         px /= repeat;
         py /= repeat;
      }

      //set the width and height
      paint_shader->setLocalParam(1, nx, ny, 0.0f, 0.0f);

      double x, y;
      double radius1;
      for (int i=0; i<=repeat; i++)
      {
         x = spx + i*px;
         y = spy + i*py;
         switch (m_selector.GetMode())
         {
         case 3:
            radius1 = m_brush_radius2;
            break;
         case 4:
            radius1 = 0.0;
            break;
         default:
            radius1 = m_brush_radius1;
            break;
         }
         //send uniforms to paint shader
         paint_shader->setLocalParam(0,
               x, double(ny)-y,
               radius1*pressure,
               m_brush_radius2*pressure);
         //draw a square
         glBegin(GL_QUADS);
         glTexCoord2f(0.0f, 0.0f);
         glVertex3f(-1.0f, -1.0f, 0.0f);
         glTexCoord2f(1.0f, 0.0f);
         glVertex3f(1.0f, -1.0f, 0.0f);
         glTexCoord2f(1.0f, 1.0f);
         glVertex3f(1.0f, 1.0f, 0.0f);
         glTexCoord2f(0.0f, 1.0f);
         glVertex3f(-1.0f, 1.0f, 0.0f);
         glEnd();
      }

      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      //release paint shader
      if (paint_shader && paint_shader->valid())
      {
         paint_shader->release();
      }
   }

   //bind back the window frame buffer
   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

//show the stroke buffer
void VRenderGLView::DisplayStroke()
{
   //painting texture
	if (!glIsTexture(m_tex_paint))
		return;

	//draw the final buffer to the windows buffer
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_tex_paint);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	ShaderProgram* img_shader =
		m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
	if (img_shader)
	{
		if (!img_shader->valid())
			img_shader->create();
		img_shader->bind();
	}
	DrawViewQuad();
	if (img_shader && img_shader->valid())
		img_shader->release();

	glEnable(GL_DEPTH_TEST);
}

//set 2d weights
void VRenderGLView::Set2dWeights()
{
   m_selector.Set2DWeight(m_tex_final, glIsTexture(m_tex_wt2)?m_tex_wt2:m_tex);
}

VolumeData *VRenderGLView::CopyLevel(VolumeData *src, int lv)
{
	if (!src->isBrxml()) return NULL;

	bool swap_selection = (m_cur_vol == src);

	VolumeData *vd_new = src->CopyLevel(lv);
	VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
	DataManager* mgr = vr_frame->GetDataManager();
	if (vd_new && vr_frame && mgr)
	{
		wxString group_name;
		DataGroup* group = 0;

		for (int i=(int)m_layer_list.size()-1; i>=0; i--)
		{
			if (!m_layer_list[i]) continue;

			if (m_layer_list[i]->IsA() == 5) //group
			{
				DataGroup* group = (DataGroup*)m_layer_list[i];
				for (int j=group->GetVolumeNum()-1; j>=0; j--)
				{
					VolumeData* gvd = group->GetVolumeData(j);
					if (gvd == src)
					{
						group_name = group->GetName();
						break;
					}
				}
			}
		}
		if (group_name.IsEmpty())
		{
			group_name = AddGroup("");
			group = GetGroup(group_name);
		}

		wxString src_name = src->GetName();
		ReplaceVolumeData(src_name, vd_new);
		mgr->ReplaceVolumeData(mgr->GetVolumeIndex(src_name), vd_new);
		
		vd_new->SetName(src_name);
		if (swap_selection) m_cur_vol = vd_new;
		SetVolPopDirty();
		PopVolumeList();
		vr_frame->UpdateList();
		//vr_frame->UpdateTree(swap_selection ? vd_new->GetName() : "", false); //UpdateTree line1: m_tree_panel->DeleteAll(); <-memory access violation
		if (swap_selection) vr_frame->OnSelection(2, m_vrv, group, vd_new);
	}

	return vd_new;
}

//segment volumes in current view
void VRenderGLView::Segment()
{
	glViewport(0, 0, (GLint)GetSize().x, (GLint)GetSize().y);
	HandleCamera();

	//translate object
	m_mv_mat = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	//rotate object
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	//center object
	m_mv_mat = glm::translate(m_mv_mat, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

	m_selector.Set2DMask(m_tex_paint);
	m_selector.Set2DWeight(m_tex_final, glIsTexture(m_tex_wt2)?m_tex_wt2:m_tex);
	//orthographic
	m_selector.SetOrthographic(!m_persp);

	bool copied = false;
	
	if (m_selector.GetSelectGroup())
	{
		VolumeData* vd = m_selector.GetVolume();
		DataGroup* group = 0;
		
		if (vd)
		{
			for (int i=0; i<GetLayerNum(); i++)
			{
				TreeLayer* layer = GetLayer(i);
				if (layer && layer->IsA() == 5)
				{
					DataGroup* tmp_group = (DataGroup*)layer;
					for (int j=0; j<tmp_group->GetVolumeNum(); j++)
					{
						VolumeData* tmp_vd = tmp_group->GetVolumeData(j);
						if (tmp_vd && tmp_vd==vd)
						{
							group = tmp_group;
							break;
						}
					}
				}
				if (group)
					break;
			}
			//select the group
			if (group && group->GetVolumeNum()>1)
			{
				vector<VolumeData *> cp_vd_list;
				VolumeData* tmp_vd;
				for (int i=0; i<group->GetVolumeNum(); i++)
				{
					tmp_vd = group->GetVolumeData(i);
					if (tmp_vd && tmp_vd->isBrxml())
						cp_vd_list.push_back(tmp_vd);
				}
				for (auto v : cp_vd_list)
				{
					bool cur = (v == vd);
					v = CopyLevel(v);
					if (cur) vd = v;
				}

				for (int i=0; i<group->GetVolumeNum(); i++)
				{
					tmp_vd = group->GetVolumeData(i);
					if (tmp_vd && tmp_vd->GetDisp())
					{
						tmp_vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
						m_selector.SetVolume(tmp_vd);
						m_selector.Select(m_brush_radius2-m_brush_radius1);
					}
				}
				m_selector.SetVolume(vd);
			}
		}
	}
	else if (m_selector.GetSelectBoth())
	{
		VolumeData* vd = m_calculator.GetVolumeA();
		if (vd && vd->isBrxml())
		{
			vd = CopyLevel(vd);
			m_calculator.SetVolumeA(vd);
			if (vd) copied = true;
		}

		if (vd)
		{
			vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
			m_selector.SetVolume(vd);
			m_selector.Select(m_brush_radius2-m_brush_radius1);
		}


		vd = m_calculator.GetVolumeB();
		if (vd && vd->isBrxml())
		{
			vd = CopyLevel(vd);
			m_calculator.SetVolumeB(vd);
			if (vd) copied = true;
		}

		if (vd)
		{
			vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
			m_selector.SetVolume(vd);
			m_selector.Select(m_brush_radius2-m_brush_radius1);
		}
	}
	else
	{
		VolumeData* vd = m_selector.GetVolume();
		if (vd && vd->isBrxml())
		{
			vd = CopyLevel(vd);
			m_selector.SetVolume(vd);
			if (vd) copied = true;
		}
		m_selector.Select(m_brush_radius2-m_brush_radius1);
	}

	if (copied = true) RefreshGL(false, true);

}

//label volumes in current view
void VRenderGLView::Label()
{
   m_selector.Label(0);
}

//remove noise
int VRenderGLView::CompAnalysis(double min_voxels, double max_voxels, double thresh, bool select, bool gen_ann)
{
   int return_val = 0;

   bool copied = false;
   VolumeData *vd = m_selector.GetVolume();
   if (vd && vd->isBrxml())
   {
	   vd = CopyLevel(vd);
	   m_selector.SetVolume(vd);
	   if (vd) copied = true;
   }

   if (!select)
   {
      m_selector.Set2DMask(m_tex_paint);
      m_selector.Set2DWeight(m_tex_final, glIsTexture(m_tex_wt2)?m_tex_wt2:m_tex);
      return_val = m_selector.CompAnalysis(min_voxels, max_voxels, thresh, 1.0, select, gen_ann);
   }
   else
      return_val = m_selector.CompAnalysis(min_voxels, max_voxels, thresh, 1.0, select, gen_ann);

   Annotations* ann = m_selector.GetAnnotations();
   if (ann)
   {
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame)
      {
         DataManager* mgr = vr_frame->GetDataManager();
         if (mgr)
            mgr->AddAnnotations(ann);
         vr_frame->UpdateList();
      }
   }

   if (copied) RefreshGL(false, true);

   return return_val;
}

void VRenderGLView::CompExport(int mode, bool select)
{
   switch (mode)
   {
   case 0://multi channels
      m_selector.CompExportMultiChann(select);
      break;
   case 1://random colors
      {
         wxString sel_name = m_selector.GetVolume()->GetName();
         VolumeData* vd_r = 0;
         VolumeData* vd_g = 0;
         VolumeData* vd_b = 0;
         VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
         if (vr_frame)
         {
            for (int i=0; i<vr_frame->GetDataManager()->GetVolumeNum(); i++)
            {
               VolumeData* vd = vr_frame->GetDataManager()->GetVolumeData(i);
               wxString name = vd->GetName();
               if (name==sel_name+"_COMP1")
                  vd_r = vd;
               else if (name==sel_name+"_COMP2")
                  vd_g = vd;
               else if (name==sel_name+"_COMP3")
                  vd_b = vd;
            }
         }
         m_selector.CompExportRandomColor(vd_r, vd_g, vd_b, select);
      }
      break;
   }
   vector<VolumeData*> *vol_list = m_selector.GetResultVols();
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (vr_frame)
   {
      wxString group_name = "";
      DataGroup* group = 0;
      for (int i=0; i<(int)vol_list->size(); i++)
      {
         VolumeData* vd = (*vol_list)[i];
         if (vd)
         {
            vr_frame->GetDataManager()->AddVolumeData(vd);
            //vr_frame->GetDataManager()->SetVolumeDefault(vd);
            if (i==0)
            {
               group_name = AddGroup("");
               group = GetGroup(group_name);
            }
            AddVolumeData(vd, group_name);
         }
      }
      if (group)
      {
         group->SetSyncRAll(true);
         group->SetSyncGAll(true);
         group->SetSyncBAll(true);
         VolumeData* vd = m_selector.GetVolume();
         if (vd)
         {
            FLIVR::Color col = vd->GetGamma();
            group->SetGammaAll(col);
            col = vd->GetBrightness();
            group->SetBrightnessAll(col);
            col = vd->GetHdr();
            group->SetHdrAll(col);
         }
      }
      vr_frame->UpdateList();
      vr_frame->UpdateTree(m_selector.GetVolume()->GetName());
      RefreshGL();
   }
}

void VRenderGLView::ShowAnnotations()
{
   Annotations* ann = m_selector.GetAnnotations();
   if (ann)
   {
      AddAnnotations(ann);
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame)
         vr_frame->UpdateTree(vr_frame->GetCurSelVol()->GetName());
   }
}

int VRenderGLView::NoiseAnalysis(double min_voxels, double max_voxels, double thresh)
{
   int return_val = 0;

   bool copied = false;
   VolumeData *vd = m_selector.GetVolume();
   if (vd && vd->isBrxml())
   {
	   vd = CopyLevel(vd);
	   m_selector.SetVolume(vd);
	   if (vd) copied = true;
   }

   m_selector.Set2DMask(m_tex_paint);
   m_selector.Set2DWeight(m_tex_final, glIsTexture(m_tex_wt2)?m_tex_wt2:m_tex);
   return_val = m_selector.NoiseAnalysis(min_voxels, max_voxels, 10.0, thresh);

   if (copied) RefreshGL(false, true);

   return return_val;
}

void VRenderGLView::NoiseRemoval(int iter, double thresh)
{
   VolumeData* vd = m_selector.GetVolume();
   if (!vd || !vd->GetMask()) return;

   //if(!vd->GetMask()) NoiseAnalysis(0.0, iter, thresh);

   wxString name = vd->GetName();
   if (name.Find("_NR")==wxNOT_FOUND)
   {
      m_selector.NoiseRemoval(iter, thresh, 1);
      vector<VolumeData*> *vol_list = m_selector.GetResultVols();
	  if(!vol_list || vol_list->empty()) return;
      VolumeData* vd_new = (*vol_list)[0];
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vd_new && vr_frame)
      {
         vr_frame->GetDataManager()->AddVolumeData(vd_new);
         wxString group_name = AddGroup("");
         AddVolumeData(vd_new, group_name);
         vd->SetDisp(false);
         vr_frame->UpdateList();
         vr_frame->UpdateTree(vd_new->GetName());
      }
   }
   else
      m_selector.NoiseRemoval(iter, thresh);
}

//brush properties
//load default
void VRenderGLView::LoadDefaultBrushSettings()
{
wxString expath = wxStandardPaths::Get().GetExecutablePath();
	expath = expath.BeforeLast(GETSLASH(),NULL);
#ifdef _WIN32
	wxString dft = expath + "\\default_brush_settings.dft";
#else
	wxString dft = expath + "/../Resources/default_brush_settings.dft";
#endif
	wxFileInputStream is(dft);
	if (!is.IsOk())
		return;
	wxFileConfig fconfig(is);

   double val;
   int ival;
   bool bval;

   //brush properties
   if (fconfig.Read("brush_ini_thresh", &val))
      m_selector.SetBrushIniThresh(val);
   if (fconfig.Read("brush_gm_falloff", &val))
      m_selector.SetBrushGMFalloff(val);
   if (fconfig.Read("brush_scl_falloff", &val))
      m_selector.SetBrushSclFalloff(val);
   if (fconfig.Read("brush_scl_translate", &val))
      m_selector.SetBrushSclTranslate(val);
   //edge detect
   if (fconfig.Read("edge_detect", &bval))
      m_selector.SetEdgeDetect(bval);
   //hidden removal
   if (fconfig.Read("hidden_removal", &bval))
      m_selector.SetHiddenRemoval(bval);
   //select group
   if (fconfig.Read("select_group", &bval))
      m_selector.SetSelectGroup(bval);
   //2d influence
   if (fconfig.Read("brush_2dinfl", &val))
      m_selector.SetW2d(val);
   //size 1
   if (fconfig.Read("brush_size1", &val) && val>0.0)
      m_brush_radius1 = val;
   //size 2 link
   if (fconfig.Read("use_brush_size2", &bval))
   {
      m_use_brush_radius2 = bval;
      m_selector.SetUseBrushSize2(bval);
   }
   //size 2
   if (fconfig.Read("brush_size2", &val) && val>0.0)
      m_brush_radius2 = val;
   //iterations
   if (fconfig.Read("brush_iters", &ival))
   {
      switch (ival)
      {
      case 1:
         m_selector.SetBrushIteration(BRUSH_TOOL_ITER_WEAK);
         break;
      case 2:
         m_selector.SetBrushIteration(BRUSH_TOOL_ITER_NORMAL);
         break;
      case 3:
         m_selector.SetBrushIteration(BRUSH_TOOL_ITER_STRONG);
         break;
      }
   }
}

void VRenderGLView::SetBrushUsePres(bool pres)
{
   m_use_pres = pres;
}

bool VRenderGLView::GetBrushUsePres()
{
   return m_use_pres;
}

void VRenderGLView::SetUseBrushSize2(bool val)
{
   m_use_brush_radius2 = val;
   m_selector.SetUseBrushSize2(val);
   if (!val)
      m_brush_radius2 = m_brush_radius1;
}

bool VRenderGLView::GetBrushSize2Link()
{
   return m_use_brush_radius2;
}

void VRenderGLView::SetBrushSize(double size1, double size2)
{
   if (size1 > 0.0)
      m_brush_radius1 = size1;
   if (size2 > 0.0)
      m_brush_radius2 = size2;
   if (!m_use_brush_radius2)
      m_brush_radius2 = m_brush_radius1;
}

double VRenderGLView::GetBrushSize1()
{
   return m_brush_radius1;
}

double VRenderGLView::GetBrushSize2()
{
   return m_brush_radius2;
}

void VRenderGLView::SetBrushSpacing(double spacing)
{
   if (spacing > 0.0)
      m_brush_spacing = spacing;
}

double VRenderGLView::GetBrushSpacing()
{
   return m_brush_spacing;
}

void VRenderGLView::SetBrushIteration(int num)
{
   m_selector.SetBrushIteration(num);
}

int VRenderGLView::GetBrushIteration()
{
   return m_selector.GetBrushIteration();
}

//brush falloff
void VRenderGLView::SetBrushSclTranslate(double val)
{
   m_selector.SetBrushSclTranslate(val);
}

double VRenderGLView::GetBrushSclTranslate()
{
   return m_selector.GetBrushSclTranslate();
}

void VRenderGLView::SetW2d(double val)
{
   m_selector.SetW2d(val);
}

double VRenderGLView::GetW2d()
{
   return m_selector.GetW2d();
}

//edge detect
void VRenderGLView::SetEdgeDetect(bool value)
{
   m_selector.SetEdgeDetect(value);
}

bool VRenderGLView::GetEdgeDetect()
{
   return m_selector.GetEdgeDetect();
}

//hidden removal
void VRenderGLView::SetHiddenRemoval(bool value)
{
   m_selector.SetHiddenRemoval(value);
}

bool VRenderGLView::GetHiddenRemoval()
{
   return m_selector.GetHiddenRemoval();
}

//select group
void VRenderGLView::SetSelectGroup(bool value)
{
   m_selector.SetSelectGroup(value);
}

bool VRenderGLView::GetSelectGroup()
{
   return m_selector.GetSelectGroup();
}

//select both
void VRenderGLView::SetSelectBoth(bool value)
{
   m_selector.SetSelectBoth(value);
}

bool VRenderGLView::GetSelectBoth()
{
   return m_selector.GetSelectBoth();
}

//calculations
void VRenderGLView::SetVolumeA(VolumeData* vd)
{
   m_calculator.SetVolumeA(vd);
   m_selector.SetVolume(vd);
}

void VRenderGLView::SetVolumeB(VolumeData* vd)
{
   m_calculator.SetVolumeB(vd);
}

wxString VRenderGLView::Calculate(int type, wxString prev_group, bool add)
{
   wxString result = "";

   bool copied = false;
   VolumeData* vd_A = m_calculator.GetVolumeA();
   if (vd_A && vd_A->isBrxml())
   {
	   vd_A = CopyLevel(vd_A);
	   if (vd_A) copied = true;
   }
   VolumeData* vd_B = m_calculator.GetVolumeB();
   if (vd_B && vd_B->isBrxml())
   {
	   vd_B = CopyLevel(vd_B);
	   if (vd_B) copied = true;
   }
   m_calculator.SetVolumeA(vd_A);
   m_calculator.SetVolumeB(vd_B);

   m_calculator.Calculate(type);
   VolumeData* vd = m_calculator.GetResult();
   if (vd)
   {
      if (type==1 ||
            type==2 ||
            type==3 ||
            type==4 ||
            type==5 ||
            type==6 ||
            type==8 ||
            type==9)
      {
         VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
         if (vr_frame)
         {
            //copy 2d adjust & color
            VolumeData* vd_a = m_calculator.GetVolumeA();
			if(vd_a)
            {
               //clipping planes
               vector<Plane*> *planes = vd_a->GetVR()?vd_a->GetVR()->get_planes():0;
               if (planes && vd->GetVR())
                  vd->GetVR()->set_planes(planes);
               //transfer function
               vd->Set3DGamma(vd_a->Get3DGamma());
               vd->SetBoundary(vd_a->GetBoundary());
               vd->SetOffset(vd_a->GetOffset());
               vd->SetLeftThresh(vd_a->GetLeftThresh());
               vd->SetRightThresh(vd_a->GetRightThresh());
               FLIVR::Color col = vd_a->GetColor();
               vd->SetColor(col);
               vd->SetAlpha(vd_a->GetAlpha());
               //shading
               vd->SetShading(vd_a->GetShading());
               double amb, diff, spec, shine;
               vd_a->GetMaterial(amb, diff, spec, shine);
               vd->SetMaterial(amb, diff, spec, shine);
               //shadow
               vd->SetShadow(vd_a->GetShadow());
               double shadow;
               vd_a->GetShadowParams(shadow);
               vd->SetShadowParams(shadow);
               //sample rate
               vd->SetSampleRate(vd_a->GetSampleRate());
               //2d adjusts
               col = vd_a->GetGamma();
               vd->SetGamma(col);
               col = vd_a->GetBrightness();
               vd->SetBrightness(col);
               col = vd_a->GetHdr();
               vd->SetHdr(col);
               vd->SetSyncR(vd_a->GetSyncR());
               vd->SetSyncG(vd_a->GetSyncG());
               vd->SetSyncB(vd_a->GetSyncB());
            }

            if (add)
            {
               vr_frame->GetDataManager()->AddVolumeData(vd);
               //vr_frame->GetDataManager()->SetVolumeDefault(vd);
               AddVolumeData(vd, prev_group);

               if (type==5 ||
                     type==6 ||
                     type==9)
               {
                  if (vd_a)
                     vd_a->SetDisp(false);
               }
               vr_frame->UpdateList();
               vr_frame->UpdateTree(vd->GetName());
            }
         }
         RefreshGL();
      }
      else if (type == 7)
      {
         VolumeData* vd_a = m_calculator.GetVolumeA();
         if (vd_a)
         {
            vd_a->Replace(vd);
            delete vd;
            VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
            if (vr_frame)
               vr_frame->GetPropView()->SetVolumeData(vd_a);
         }
         RefreshGL();
      }
   }

   if (copied)
   {
	   RefreshGL(false, true);
   }

   return prev_group;
}

//draw out the framebuffer after composition
void VRenderGLView::PrepFinalBuffer()
{
   int nx, ny;
   nx = GetSize().x;
   ny = GetSize().y;

   //generate textures & buffer objects
   glActiveTexture(GL_TEXTURE0);
   glEnable(GL_TEXTURE_2D);
   //frame buffer for final
   if (!glIsFramebuffer(m_fbo_final))
   {
      glGenFramebuffers(1, &m_fbo_final);
      //color buffer/texture for final
      if (!glIsTexture(m_tex_final))
         glGenTextures(1, &m_tex_final);
      //fbo_final
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_final);
      //color buffer for final
      glBindTexture(GL_TEXTURE_2D, m_tex_final);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);//GL_RGBA16F
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_tex_final, 0);
   }

   if (m_resize)
   {
      glBindTexture(GL_TEXTURE_2D, m_tex_final);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);//GL_RGBA16F
      //m_resize = false;
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VRenderGLView::ClearFinalBuffer()
{
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_final);
   //clear color buffer to black for compositing
   glClearColor(0.0, 0.0, 0.0, 0.0);
   glClear(GL_COLOR_BUFFER_BIT);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VRenderGLView::DrawFinalBuffer()
{
   //bind back the window frame buffer
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   //draw the final buffer to the windows buffer
   glActiveTexture(GL_TEXTURE0);
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, m_tex_final);
   glEnable(GL_BLEND);
   //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   //glBlendFunc(GL_ONE, GL_ONE);
   glDisable(GL_DEPTH_TEST);
   
   //2d adjustment
   ShaderProgram* img_shader =
      m_img_shader_factory.shader(IMG_SHDR_BLEND_BRIGHT_BACKGROUND_HDR);
   if (img_shader)
   {
      if (!img_shader->valid())
      {
         img_shader->create();
      }
      img_shader->bind();
   }
   img_shader->setLocalParam(0, m_gamma.r(), m_gamma.g(), m_gamma.b(), 1.0);
   img_shader->setLocalParam(1, m_brightness.r(), m_brightness.g(), m_brightness.b(), 1.0);
   img_shader->setLocalParam(2, m_hdr.r(), m_hdr.g(), m_hdr.b(), 0.0);
   //2d adjustment

   DrawViewQuad();

   if (img_shader && img_shader->valid())
      img_shader->release();
}

//Draw the volmues with compositing
//peel==true -- depth peeling
void VRenderGLView::DrawVolumesComp(vector<VolumeData*> &list, int peel, bool mask)
{
   if (list.size() <= 0)
      return;

   int i;

   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;

   //count volumes with mask
   //calculate sampling frequency factor
   int cnt_mask = 0;
   bool use_tex_wt2 = false;
   double sampling_frq_fac = -1.0;
   for (i=0; i<(int)list.size(); i++)
   {
      VolumeData* vd = list[i];
      if (!vd || !vd->GetDisp())
         continue;
	  Texture *tex = vd->GetTexture();
	  if (tex)
	  {
		  Transform *field_trans = tex->transform();
		  Vector spcv[3] = {Vector(1.0, 0.0, 0.0), Vector(0.0, 1.0, 0.0), Vector(0.0, 0.0, 1.0)};
		  double maxlen = -1;
		  for(int i = 0; i < 3 ; i++)
		  {
			  // index space view direction
			  Vector v;
			  v = field_trans->project(spcv[i]);
			  v.safe_normalize();
			  v = field_trans->project(spcv[i]);

			  double len = Dot(spcv[i], v);
			  if(len > maxlen) maxlen = len;
		  }
		  if (maxlen > sampling_frq_fac) sampling_frq_fac = maxlen;

		  if (tex->nmask()!=-1)
		  {
			  cnt_mask++;
			  if (vr_frame &&
				  vd==vr_frame->GetCurSelVol() &&
				  !mask)
				  use_tex_wt2 = true;
		  }
	  }
   }

   if (mask && cnt_mask==0)
      return;

   int nx, ny;
   nx = GetSize().x;
   ny = GetSize().y;

   //generate textures & buffer objects
   //frame buffer for each volume
   if (glIsFramebuffer(m_fbo)!=GL_TRUE)
   {
      glGenFramebuffers(1, &m_fbo);
      //color buffer/texture for each volume
      if (glIsTexture(m_tex)!=GL_TRUE)
         glGenTextures(1, &m_tex);
      //fbo
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
      //color buffer for each volume
      glBindTexture(GL_TEXTURE_2D, m_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);//GL_RGBA16F
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_tex, 0);
	  glBindFramebuffer(GL_FRAMEBUFFER, 0);
   }
   if (TextureRenderer::get_mem_swap())
   {
      if (glIsFramebuffer(m_fbo_temp)!=GL_TRUE)
      {
         glGenFramebuffers(1, &m_fbo_temp);
         if (glIsTexture(m_tex_temp)!=GL_TRUE)
            glGenTextures(1, &m_tex_temp);
         glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_temp);
         //color buffer for each volume
         glBindTexture(GL_TEXTURE_2D, m_tex_temp);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);//GL_RGBA16F
         glFramebufferTexture2D(GL_FRAMEBUFFER,
               GL_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, m_tex_temp, 0);
		 glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
   }
   if (use_tex_wt2)
   {
      if (glIsTexture(m_tex_wt2)!=GL_TRUE)
         glGenTextures(1, &m_tex_wt2);
      //color buffer for current segmented volume
      glBindTexture(GL_TEXTURE_2D, m_tex_wt2);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);
   }

   if (m_resize)
   {
      if (TextureRenderer::get_mem_swap())
      {
         glBindTexture(GL_TEXTURE_2D, m_tex_temp);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);
      }
      if (use_tex_wt2)
      {
         glBindTexture(GL_TEXTURE_2D, m_tex_wt2);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);
      }
      glBindTexture(GL_TEXTURE_2D, m_tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);//GL_RGBA16F
   }

   //draw each volume to fbo
   for (i=0; i<(int)list.size(); i++)
   {
      VolumeData* vd = list[i];
      if (!vd || !vd->GetDisp())
         continue;

	  //switchLevel(vd);

      if (mask)
      {
         //when run script
         VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
         if (vr_frame &&
               vr_frame->GetSettingDlg() &&
               vr_frame->GetSettingDlg()->GetRunScript() &&
               vd->GetTexture() &&
               vd->GetTexture()->nmask()!=-1 &&
               vd->GetTexture()->nlabel()!=-1)
            continue;

         if (vd->GetTexture() && vd->GetTexture()->nmask()!=-1)
		 {
			 Color tmp_hdr;
			 if (m_enhance_sel)
			 {
				 Color mask_color = vd->GetMaskColor();
				 double hdr_r = 0.0;
				 double hdr_g = 0.0;
				 double hdr_b = 0.0;
				 if (mask_color.r() > 0.0)
					 hdr_r = 0.4;
				 if (mask_color.g() > 0.0)
					 hdr_g = 0.4;
				 if (mask_color.b() > 0.0)
					 hdr_b = 0.4;
				 Color hdr_color = Color(hdr_r, hdr_g, hdr_b);
				 tmp_hdr = vd->GetHdr();
				 vd->SetHdr(hdr_color);
			 }

            vd->SetMaskMode(1);
            int vol_method = m_vol_method;
            m_vol_method = VOL_METHOD_COMP;
            if (vd->GetMode() == 1)
               DrawMIP(vd, m_tex, peel, sampling_frq_fac);
            else
               DrawOVER(vd, m_tex, peel, sampling_frq_fac);
            vd->SetMaskMode(0);
            m_vol_method = vol_method;

			if (m_enhance_sel) vd->SetHdr(tmp_hdr);
         }
      }
      else
      {
         GLuint tex = m_tex;
         if (vr_frame && vd==vr_frame->GetCurSelVol() &&
               vd->GetTexture() && vd->GetTexture()->nmask()!=-1)
         {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                  GL_COLOR_ATTACHMENT0,
                  GL_TEXTURE_2D, m_tex_wt2, 0);
            tex = m_tex_wt2;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
         }
         if (vd->GetBlendMode()!=2)
         {
            //when run script
            VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
            if (vr_frame &&
                  vr_frame->GetSettingDlg() &&
                  vr_frame->GetSettingDlg()->GetRunScript() &&
                  vd->GetTexture() &&
                  vd->GetTexture()->nmask()!=-1 &&
                  vd->GetTexture()->nlabel()!=-1)
               vd->SetMaskMode(4);

            if (vd->GetMode() == 1)
               DrawMIP(vd, tex, peel, sampling_frq_fac);
            else
               DrawOVER(vd, tex, peel, sampling_frq_fac);
         }
         if (tex==m_tex_wt2)
         {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                  GL_COLOR_ATTACHMENT0,
                  GL_TEXTURE_2D, m_tex, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
         }
      }
   }
}

void VRenderGLView::switchLevel(VolumeData *vd)
{
	if(!vd) return;

	int nx, ny;
	nx = GetSize().x;
	ny = GetSize().y;

	if(m_min_ppi < 0)m_min_ppi = 60;
	//wxDisplay disp(wxDisplay::GetFromWindow(m_frame));
	wxSize disp_ppi = wxGetDisplayPPI();
	double disp_ppi_x = m_min_ppi;
	double disp_ppi_y = m_min_ppi;
	if(disp_ppi.GetX() > 0)disp_ppi_x = disp_ppi.GetX();
	if(disp_ppi.GetY() > 0)disp_ppi_y = disp_ppi.GetY();
		
	Texture *vtex = vd->GetTexture();
	if (vtex && vtex->isBrxml())
	{
		vector<double> sfs;
		vector<double> spx, spy, spz;
		int lvnum = vtex->GetLevelNum();
		for (int i = 0; i < lvnum; i++)
		{
			double aspect = (double)nx / (double)ny;

			double spc_x;
			double spc_y;
			double spc_z;
			vtex->get_spacings(spc_x, spc_y, spc_z, i);
			spc_x = spc_x<EPS?1.0:spc_x;
			spc_y = spc_y<EPS?1.0:spc_y;

			spx.push_back(spc_x);
			spy.push_back(spc_y);
			spz.push_back(spc_z);

			double sf;
			if (aspect > 1.0)
				sf = 2.0*m_radius*disp_ppi_y/spc_y/double(ny)/m_min_ppi;
			else
				sf = 2.0*m_radius*disp_ppi_x/spc_x/double(nx)/m_min_ppi;
			sfs.push_back(sf);
		}
		
		int lv = lvnum - 1;
		if (!m_manip)
		{
			for (int i = lvnum - 1; i >= 0; i--)
			{
				if (m_scale_factor > ( m_int_res ? sfs[i]*16.0 : sfs[i])) lv = i - 1;
			}
		}
		if (lv < 0) lv = 0;
		//if (m_interactive) lv += 2;
		if (lv >= lvnum) lv = lvnum - 1;
		vd->SetLevel(lv);
		//vd->GetVR()->set_scalar_scale();
	}
}

void VRenderGLView::DrawOVER(VolumeData* vd, GLuint tex, int peel, double sampling_frq_fac)
{
	ShaderProgram* img_shader = 0;

	bool do_over = true;
	if (TextureRenderer::get_mem_swap() &&
		TextureRenderer::get_start_update_loop() &&
		!TextureRenderer::get_done_update_loop())
	{
		unsigned int rn_time = GET_TICK_COUNT();
		if (rn_time - TextureRenderer::get_st_time() >
			TextureRenderer::get_up_time())
			return;
		if (vd->GetVR()->get_done_loop(0) && 
			( !vd->isActiveMask()  || vd->GetVR()->get_done_loop(TEXTURE_RENDER_MODE_MASK)  ) &&
			( !vd->isActiveLabel() || vd->GetVR()->get_done_loop(TEXTURE_RENDER_MODE_LABEL) )   )
			do_over = false;
	}

	if (do_over)
	{
		//before rendering this channel, save m_fbo_final to m_fbo_temp
		if (TextureRenderer::get_mem_swap() &&
			TextureRenderer::get_start_update_loop() &&
			TextureRenderer::get_save_final_buffer())
		{
			TextureRenderer::reset_save_final_buffer();

			glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_temp);
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_tex_final);
			glDisable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);

			img_shader =
				m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
			if (img_shader)
			{
				if (!img_shader->valid())
					img_shader->create();
				img_shader->bind();
			}
			DrawViewQuad();
			if (img_shader && img_shader->valid())
				img_shader->release();

			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		//bind the fbo
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

		if (!TextureRenderer::get_mem_swap() ||
			(TextureRenderer::get_mem_swap() &&
			TextureRenderer::get_clear_chan_buffer()))
		{
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
			TextureRenderer::reset_clear_chan_buffer();
		}

		if (vd->GetVR())
			vd->GetVR()->set_depth_peel(peel);
		vd->SetStreamMode(0);
		vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
		vd->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
		vd->Draw(!m_persp, m_interactive, m_scale_factor, m_intp, sampling_frq_fac);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (vd->GetShadow())
	{
		vector<VolumeData*> list;
		list.push_back(vd);
		DrawOLShadows(list, tex);
	}

	//bind fbo for final composition
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_final);

	if (TextureRenderer::get_mem_swap())
	{
		//restore m_fbo_temp to m_fbo_final
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_tex_temp);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);

		img_shader =
			m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
		if (img_shader)
		{
			if (!img_shader->valid())
				img_shader->create();
			img_shader->bind();
		}
		DrawViewQuad();
		if (img_shader && img_shader->valid())
			img_shader->release();

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	//build mipmap
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glEnable(GL_BLEND);
	if (m_vol_method == VOL_METHOD_COMP)
		glBlendFunc(GL_ONE, GL_ONE);
	else
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	//2d adjustment
	img_shader =
		m_img_shader_factory.shader(IMG_SHDR_BRIGHTNESS_CONTRAST_HDR);
	if (img_shader)
	{
		if (!img_shader->valid())
		{
			img_shader->create();
		}
		img_shader->bind();
	}

	Color gamma = vd->GetGamma();
	Color brightness = vd->GetBrightness();
	img_shader->setLocalParam(0, gamma.r(), gamma.g(), gamma.b(), 1.0);
	img_shader->setLocalParam(1, brightness.r(), brightness.g(), brightness.b(), 1.0);
	Color hdr = vd->GetHdr();
	img_shader->setLocalParam(2, hdr.r(), hdr.g(), hdr.b(), 0.0);
	//2d adjustment

	DrawViewQuad();

	if (img_shader && img_shader->valid())
		img_shader->release();

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VRenderGLView::DrawMIP(VolumeData* vd, GLuint tex, int peel, double sampling_frq_fac)
{
   bool do_mip = true;
   if (TextureRenderer::get_mem_swap() &&
         TextureRenderer::get_start_update_loop() &&
         !TextureRenderer::get_done_update_loop())
   {
      unsigned int rn_time = GET_TICK_COUNT();
      if (rn_time - TextureRenderer::get_st_time() >
            TextureRenderer::get_up_time())
         return;
      if (vd->GetVR()->get_done_loop(1) && 
		  ( !vd->isActiveMask()  || vd->GetVR()->get_done_loop(TEXTURE_RENDER_MODE_MASK)  ) &&
		  ( !vd->isActiveLabel() || vd->GetVR()->get_done_loop(TEXTURE_RENDER_MODE_LABEL) )   )
         do_mip = false;
   }

   int nx, ny;
   nx = GetSize().x;
   ny = GetSize().y;

   bool shading = vd->GetVR()->get_shading();
   bool shadow = vd->GetShadow();
   int color_mode = vd->GetColormapMode();
   bool enable_alpha = vd->GetEnableAlpha();
   ShaderProgram* img_shader = 0;

   if (do_mip)
   {
	   //before rendering this channel, save m_fbo_final to m_fbo_temp
	   if (TextureRenderer::get_mem_swap() &&
		   TextureRenderer::get_start_update_loop() &&
		   TextureRenderer::get_save_final_buffer())
	   {
		   TextureRenderer::reset_save_final_buffer();

		   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_temp);
		   glClearColor(0.0, 0.0, 0.0, 0.0);
		   glClear(GL_COLOR_BUFFER_BIT);
		   glActiveTexture(GL_TEXTURE0);
		   glBindTexture(GL_TEXTURE_2D, m_tex_final);
		   glDisable(GL_BLEND);
		   glDisable(GL_DEPTH_TEST);

		   img_shader =
			   m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
		   if (img_shader)
		   {
			   if (!img_shader->valid())
				   img_shader->create();
			   img_shader->bind();
		   }
		   DrawViewQuad();
		   if (img_shader && img_shader->valid())
			   img_shader->release();

		   glBindTexture(GL_TEXTURE_2D, 0);
		   glBindFramebuffer(GL_FRAMEBUFFER, 0);
	   }

      if (!glIsFramebuffer(m_fbo_ol1))
      {
         glGenFramebuffers(1, &m_fbo_ol1);
         if (!glIsTexture(m_tex_ol1))
            glGenTextures(1, &m_tex_ol1);
         glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol1);
         glBindTexture(GL_TEXTURE_2D, m_tex_ol1);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);
         glFramebufferTexture2D(GL_FRAMEBUFFER,
               GL_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, m_tex_ol1, 0);
      }

      if (m_resize_ol1)
      {
         glBindTexture(GL_TEXTURE_2D, m_tex_ol1);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);
         m_resize_ol1 = false;
      }

      //bind the fbo
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol1);

      if (!TextureRenderer::get_mem_swap() ||
            (TextureRenderer::get_mem_swap() &&
             TextureRenderer::get_clear_chan_buffer()))
      {
         glClearColor(0.0, 0.0, 0.0, 0.0);
         glClear(GL_COLOR_BUFFER_BIT);
         TextureRenderer::reset_clear_chan_buffer();
      }

      if (vd->GetVR())
         vd->GetVR()->set_depth_peel(peel);
      vd->GetVR()->set_shading(false);
      GLboolean use_fog = glIsEnabled(GL_FOG);
	  if (color_mode == 1)
	  {
		  vd->SetMode(3);
		  vd->SetFog(false, m_fog_intensity, m_fog_start, m_fog_end);
	  }
	  else
	  {
		  vd->SetMode(1);
		  vd->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
	  }
	  //turn off alpha
	  if (color_mode == 1)
		  vd->SetEnableAlpha(false);
	  //draw
      vd->SetStreamMode(1);
	  vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
      vd->Draw(!m_persp, m_interactive, m_scale_factor, m_intp, sampling_frq_fac);
	  
      //
      if (color_mode == 1)
      {
         vd->RestoreMode();
         //restore alpha
         vd->SetEnableAlpha(enable_alpha);
      }

	  glBindTexture(GL_TEXTURE_2D, 0);
	  glBindFramebuffer(GL_FRAMEBUFFER, 0);

	  //bind fbo for final composition
	  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	  glClearColor(0.0, 0.0, 0.0, 0.0);
	  glClear(GL_COLOR_BUFFER_BIT);
	  glActiveTexture(GL_TEXTURE0);
	  glBindTexture(GL_TEXTURE_2D, m_tex_ol1);
	  glEnable(GL_BLEND);
	  glBlendEquation(GL_FUNC_ADD);
	  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	  if (color_mode == 1)
	  {
		  //2d adjustment
		  img_shader =
			  m_img_shader_factory.shader(IMG_SHDR_GRADIENT_MAP);
		  if (img_shader)
		  {
			  if (!img_shader->valid())
			  {
				  img_shader->create();
			  }
			  img_shader->bind();
		  }
		  double lo, hi;
		  vd->GetColormapValues(lo, hi);
		  img_shader->setLocalParam(0, lo, hi, hi-lo, enable_alpha?0.0:1.0);
		  //2d adjustment
	  }
	  else
	  {
		  img_shader =
			  m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
	  }

	  if (img_shader)
	  {
		  if (!img_shader->valid())
			  img_shader->create();
		  img_shader->bind();
	  }
	  DrawViewQuad();
	  if (img_shader && img_shader->valid())
		  img_shader->release();

	  if (color_mode == 1 &&
		  img_shader &&
		  img_shader->valid())
	  {
		  img_shader->release();
	  }

	  glBindTexture(GL_TEXTURE_2D, 0);
	  glBindFramebuffer(GL_FRAMEBUFFER, 0);
   }

   if (shading)
   {
      DrawOLShading(vd);
   }

   if (shadow)
   {
      vector<VolumeData*> list;
      list.push_back(vd);
      DrawOLShadows(list, tex);
   }

   //bind fbo for final composition
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_final);

   if (TextureRenderer::get_mem_swap())
   {
      //restore m_fbo_temp to m_fbo_final
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_tex_temp);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);

		img_shader =
			m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
		if (img_shader)
		{
			if (!img_shader->valid())
				img_shader->create();
			img_shader->bind();
		}
		DrawViewQuad();
		if (img_shader && img_shader->valid())
			img_shader->release();

		glBindTexture(GL_TEXTURE_2D, 0);
   }

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, tex);
   //build mipmap
   glGenerateMipmap(GL_TEXTURE_2D);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   glEnable(GL_BLEND);
   if (m_vol_method == VOL_METHOD_COMP)
	   glBlendFunc(GL_ONE, GL_ONE);
   else
	   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_DEPTH_TEST);

   //2d adjustment
   img_shader =
      m_img_shader_factory.shader(IMG_SHDR_BRIGHTNESS_CONTRAST_HDR);
   if (img_shader)
   {
      if (!img_shader->valid())
         img_shader->create();
      img_shader->bind();
   }
   Color gamma = vd->GetGamma();
   Color brightness = vd->GetBrightness();
   img_shader->setLocalParam(0, gamma.r(), gamma.g(), gamma.b(), 1.0);
   img_shader->setLocalParam(1, brightness.r(), brightness.g(), brightness.b(), 1.0);
   Color hdr = vd->GetHdr();
   img_shader->setLocalParam(2, hdr.r(), hdr.g(), hdr.b(), 0.0);
   //2d adjustment

   DrawViewQuad();

   if (img_shader && img_shader->valid())
      img_shader->release();

   vd->GetVR()->set_shading(shading);
   vd->SetColormapMode(color_mode);
}

void VRenderGLView::DrawOLShading(VolumeData* vd)
{
   if (TextureRenderer::get_mem_swap() &&
         TextureRenderer::get_start_update_loop() &&
         !TextureRenderer::get_done_update_loop())
   {
      unsigned int rn_time = GET_TICK_COUNT();
      if (rn_time - TextureRenderer::get_st_time() >
            TextureRenderer::get_up_time())
         return;
      if (vd->GetVR()->get_done_loop(2))
         return;
   }

   //shading pass
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol1);
   glClearColor(1.0, 1.0, 1.0, 1.0);
   glClear(GL_COLOR_BUFFER_BIT);

   vd->GetVR()->set_shading(true);
   vd->SetMode(2);
   int colormode = vd->GetColormapMode();
   vd->SetStreamMode(2);
   vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
   vd->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
   vd->Draw(!m_persp, m_interactive, m_scale_factor, m_intp);
   vd->RestoreMode();
   vd->SetColormapMode(colormode);

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   //bind fbo for final composition
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, m_tex_ol1);
   glEnable(GL_BLEND);
   glBlendFunc(GL_ZERO, GL_SRC_COLOR);
   //glBlendEquation(GL_MIN);
   glDisable(GL_DEPTH_TEST);

   ShaderProgram* img_shader =
	   m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
   if (img_shader)
   {
	   if (!img_shader->valid())
		   img_shader->create();
	   img_shader->bind();
   }
   DrawViewQuad();
   if (img_shader && img_shader->valid())
	   img_shader->release();
   
   glBlendEquation(GL_FUNC_ADD);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//get mesh shadow
bool VRenderGLView::GetMeshShadow(double &val)
{
   for (int i=0 ; i<(int)m_layer_list.size() ; i++)
   {
      if (!m_layer_list[i])
         continue;
      if (m_layer_list[i]->IsA() == 3)
      {
         MeshData* md = (MeshData*)m_layer_list[i];
         if (md && md->GetDisp())
         {
            md->GetShadowParams(val);
            return md->GetShadow();
         }
      }
      else if (m_layer_list[i]->IsA() == 6)
      {
         MeshGroup* group = (MeshGroup*)m_layer_list[i];
         if (group && group->GetDisp())
         {
            for (int j=0; j<(int)group->GetMeshNum(); j++)
            {
               MeshData* md = group->GetMeshData(j);
               if (md && md->GetDisp())
               {
                  md->GetShadowParams(val);
                  return md->GetShadow();
               }
            }
         }
      }
   }
   val = 0.0;
   return false;
}

void VRenderGLView::DrawOLShadowsMesh(GLuint tex_depth, double darkness)
{
   int nx, ny;
   nx = GetSize().x;
   ny = GetSize().y;

   //shadow pass
   if (!glIsFramebuffer(m_fbo_ol2))
   {
      glGenFramebuffers(1, &m_fbo_ol2);
      if (!glIsTexture(m_tex_ol2))
         glGenTextures(1, &m_tex_ol2);
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol2);
      glBindTexture(GL_TEXTURE_2D, m_tex_ol2);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_tex_ol2, 0);
   }

   if (m_resize_ol2)
   {
      glBindTexture(GL_TEXTURE_2D, m_tex_ol2);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);
      m_resize_ol2 = false;
   }

   //bind the fbo
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol2);
   glClearColor(1.0, 1.0, 1.0, 1.0);
   glClear(GL_COLOR_BUFFER_BIT);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, tex_depth);
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);

   //2d adjustment
   ShaderProgram* img_shader =
	   m_img_shader_factory.shader(IMG_SHDR_DEPTH_TO_GRADIENT);
   if (img_shader)
   {
	   if (!img_shader->valid())
	   {
		   img_shader->create();
	   }
	   img_shader->bind();
   }
   img_shader->setLocalParam(0, 1.0/nx, 1.0/ny, m_persp?2e10:1e6, 0.0);
   double dir_x = 0.0, dir_y = 0.0;
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (vr_frame && vr_frame->GetSettingDlg())
	   vr_frame->GetSettingDlg()->GetShadowDir(dir_x, dir_y);
   img_shader->setLocalParam(1, dir_x, dir_y, 0.0, 0.0);
   //2d adjustment

   DrawViewQuad();

   if (img_shader && img_shader->valid())
   {
	   img_shader->release();
   }

   //
   //bind fbo for final composition
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, m_tex_ol2);
   glEnable(GL_BLEND);
   glBlendFunc(GL_ZERO, GL_SRC_COLOR);
   glDisable(GL_DEPTH_TEST);

   //2d adjustment
   img_shader =
      m_img_shader_factory.shader(IMG_SHDR_GRADIENT_TO_SHADOW_MESH);
   if (img_shader)
   {
      if (!img_shader->valid())
      {
         img_shader->create();
      }
      img_shader->bind();
   }
   img_shader->setLocalParam(0, 1.0/nx, 1.0/ny, max(m_scale_factor, 1.0), 0.0);
   img_shader->setLocalParam(1, darkness, 0.0, 0.0, 0.0);
   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, tex_depth);
   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, m_tex_final);
   //2d adjustment

   DrawViewQuad();

   if (img_shader && img_shader->valid())
   {
      img_shader->release();
   }
   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, 0);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, 0);
   
   glBlendEquation(GL_FUNC_ADD);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_DEPTH_TEST);
   glEnable(GL_LIGHTING);
}

void VRenderGLView::DrawOLShadows(vector<VolumeData*> &vlist, GLuint tex)
{
   if (vlist.empty())
      return;

   if (TextureRenderer::get_mem_swap() &&
	   TextureRenderer::get_start_update_loop())
   {
	   if (TextureRenderer::get_done_update_loop())
		   return;
	   else if (vlist.size() == 1 && vlist[0]->GetShadow())
		   if (vlist[0]->GetVR()->get_done_loop(3))
			   return;
   }
   
   int nx, ny;
   nx = GetSize().x;
   ny = GetSize().y;

   //gradient pass
   if (!glIsFramebuffer(m_fbo_ol1))
   {
      glGenFramebuffers(1, &m_fbo_ol1);
      if (!glIsTexture(m_tex_ol1))
         glGenTextures(1, &m_tex_ol1);
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol1);
      glBindTexture(GL_TEXTURE_2D, m_tex_ol1);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_tex_ol1, 0);
   }

   if (m_resize_ol1)
   {
      glBindTexture(GL_TEXTURE_2D, m_tex_ol1);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);
      m_resize_ol1 = false;
   }

   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol1);
   if (!TextureRenderer::get_mem_swap() ||
         (TextureRenderer::get_mem_swap() &&
          TextureRenderer::get_clear_chan_buffer()))
   {
      glClearColor(1.0, 1.0, 1.0, 1.0);
      glClear(GL_COLOR_BUFFER_BIT);
      TextureRenderer::reset_clear_chan_buffer();
   }
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);

   double shadow_darkness = 0.0;

   if (vlist.size() == 1 && vlist[0]->GetShadow())
   {
      VolumeData* vd = vlist[0];

	  if (vd->GetTexture())
	  {
		  //save
		  int colormode = vd->GetColormapMode();
		  bool shading = vd->GetVR()->get_shading();
		  //set to draw depth
		  vd->GetVR()->set_shading(false);
		  vd->SetMode(0);
		  vd->SetColormapMode(2);
		  vd->Set2dDmap(m_tex_ol1);
		  //draw
		  vd->SetStreamMode(3);
		  int uporder = TextureRenderer::get_update_order();
		  TextureRenderer::set_update_order(0);
		  vd->GetTexture()->set_sort_bricks();
		  vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
		  vd->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
		  vd->Draw(!m_persp, m_interactive, m_scale_factor, m_intp);
		  //restore
		  vd->RestoreMode();
		  vd->SetColormapMode(colormode);
		  vd->GetVR()->set_shading(shading);
		  vd->GetShadowParams(shadow_darkness);
		  TextureRenderer::set_update_order(uporder);
		  vd->GetTexture()->set_sort_bricks();
	  }

   }
   else
   {
      vector<int> colormodes;
      vector<bool> shadings;
      vector<VolumeData*> list;

	  int uporder = TextureRenderer::get_update_order();
	  TextureRenderer::set_update_order(0);

      //geenerate list
      int i;
      for (i=0; i<(int)vlist.size(); i++)
      {
         VolumeData* vd = vlist[i];
		 if (vd && vd->GetShadow() && vd->GetTexture())
         {
            colormodes.push_back(vd->GetColormapMode());
            shadings.push_back(vd->GetVR()->get_shading());
            list.push_back(vd);
         }
      }
      if (!list.empty())
      {
         m_mvr->clear_vr();
         for (i=0; i<(int)list.size(); i++)
         {
			 VolumeData* vd = list[i];
			 vd->GetVR()->set_shading(false);
			 vd->SetMode(0);
			 vd->SetColormapMode(2);
			 vd->Set2dDmap(m_tex_ol1);
			 vd->GetTexture()->set_sort_bricks();
			 VolumeRenderer* vr = list[i]->GetVR();
			 if (vr)
			 {
				 list[i]->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
				 list[i]->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
				 m_mvr->add_vr(vr);
				 m_mvr->set_sampling_rate(vr->get_sampling_rate());
				 m_mvr->SetNoiseRed(vr->GetNoiseRed());
			 }
         }
         m_mvr->set_colormap_mode(2);
         //draw
         m_mvr->draw(m_test_wiref, m_interactive, !m_persp, m_scale_factor, m_intp);
         //restore
         m_mvr->set_colormap_mode(0);
         for (i=0; i<(int)list.size(); i++)
         {
            VolumeData* vd = list[i];
            vd->RestoreMode();
            vd->SetColormapMode(colormodes[i]);
            vd->GetVR()->set_shading(shadings[i]);
			vd->GetTexture()->set_sort_bricks();
         }
         list[0]->GetShadowParams(shadow_darkness);
      }
	  
	  TextureRenderer::set_update_order(uporder);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   //
   if (!TextureRenderer::get_mem_swap() ||
         (TextureRenderer::get_mem_swap() &&
          TextureRenderer::get_clear_chan_buffer()))
   {
      //shadow pass
      if (!glIsFramebuffer(m_fbo_ol2))
      {
         glGenFramebuffers(1, &m_fbo_ol2);
         if (!glIsTexture(m_tex_ol2))
            glGenTextures(1, &m_tex_ol2);
         glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol2);
         glBindTexture(GL_TEXTURE_2D, m_tex_ol2);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);
         glFramebufferTexture2D(GL_FRAMEBUFFER,
               GL_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, m_tex_ol2, 0);
      }

      if (m_resize_ol2)
      {
         glBindTexture(GL_TEXTURE_2D, m_tex_ol2);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);
         m_resize_ol2 = false;
      }

	  //bind the fbo
	  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_ol2);
	  glClearColor(1.0, 1.0, 1.0, 1.0);
	  glClear(GL_COLOR_BUFFER_BIT);
	  glActiveTexture(GL_TEXTURE0);
	  glBindTexture(GL_TEXTURE_2D, m_tex_ol1);
	  glDisable(GL_BLEND);
	  glDisable(GL_DEPTH_TEST);

      //2d adjustment
      ShaderProgram* img_shader =
         m_img_shader_factory.shader(IMG_SHDR_DEPTH_TO_GRADIENT);
      if (img_shader)
      {
         if (!img_shader->valid())
            img_shader->create();
         img_shader->bind();
      }
      img_shader->setLocalParam(0, 1.0/nx, 1.0/ny, m_persp?2e10:1e6, 0.0);
      double dir_x = 0.0, dir_y = 0.0;
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame && vr_frame->GetSettingDlg())
         vr_frame->GetSettingDlg()->GetShadowDir(dir_x, dir_y);
      img_shader->setLocalParam(1, dir_x, dir_y, 0.0, 0.0);
      //2d adjustment

      DrawViewQuad();

      if (img_shader && img_shader->valid())
         img_shader->release();

	  glBindTexture(GL_TEXTURE_2D, 0);
	  glBindFramebuffer(GL_FRAMEBUFFER, 0);

	  //bind fbo for final composition
	  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	  glActiveTexture(GL_TEXTURE0);
	  glBindTexture(GL_TEXTURE_2D, m_tex_ol2);
	  glEnable(GL_BLEND);
	  glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	  glDisable(GL_DEPTH_TEST);

      //2d adjustment
      img_shader =
         m_img_shader_factory.shader(IMG_SHDR_GRADIENT_TO_SHADOW);
      if (img_shader)
      {
         if (!img_shader->valid())
            img_shader->create();
         img_shader->bind();
      }
      img_shader->setLocalParam(0, 1.0/nx, 1.0/ny, max(m_scale_factor, 1.0), 0.0);
      img_shader->setLocalParam(1, shadow_darkness, 0.0, 0.0, 0.0);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, tex);
      //2d adjustment

      DrawViewQuad();

      if (img_shader && img_shader->valid())
         img_shader->release();

	  glBindTexture(GL_TEXTURE_2D, 0);
	  glBindFramebuffer(GL_FRAMEBUFFER, 0);
   }

   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, 0);
   
   glActiveTexture(GL_TEXTURE0);
   glBlendEquation(GL_FUNC_ADD);
}

//draw multi volumes with depth consideration
//peel==true -- depth peeling
void VRenderGLView::DrawVolumesMulti(vector<VolumeData*> &list, int peel)
{
   if (list.empty())
      return;

   if (!m_mvr)
      m_mvr = new MultiVolumeRenderer();
   if (!m_mvr)
      return;

   ShaderProgram* img_shader;

   m_mvr->set_blend_slices(m_blend_slices);

   int i;
   bool use_tex_wt2 = false;
   bool shadow = false;
   m_mvr->clear_vr();
   for (i=0; i<(int)list.size(); i++)
   {
	   if (list[i] && list[i]->GetDisp())
	   {

		   //switchLevel(list[i]);
		   shadow |= list[i]->GetShadow();

		   VolumeRenderer* vr = list[i]->GetVR();
		   if (vr)
		   {
			   list[i]->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);
			   list[i]->SetFog(m_use_fog, m_fog_intensity, m_fog_start, m_fog_end);
			   m_mvr->add_vr(vr);
			   m_mvr->set_sampling_rate(vr->get_sampling_rate());
			   m_mvr->SetNoiseRed(vr->GetNoiseRed());
		   }
		   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
		   if (list[i]->GetTexture() &&
			   list[i]->GetTexture()->nmask()!=-1 &&
			   vr_frame &&
			   list[i]==vr_frame->GetCurSelVol())
			   use_tex_wt2 = true;
	   }
   }

   if (m_mvr->get_vr_num()<=0)
      return;
   m_mvr->set_depth_peel(peel);

   int nx, ny;
   nx = GetSize().x;
   ny = GetSize().y;

   //generate textures & buffer objects
   //frame buffer for each volume
   if (glIsFramebuffer(m_fbo)!=GL_TRUE)
   {
      glGenFramebuffers(1, &m_fbo);
      //fbo
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
      //color buffer/texture for each volume
      if (glIsTexture(m_tex)!=GL_TRUE)
         glGenTextures(1, &m_tex);
      //color buffer for each volume
      glBindTexture(GL_TEXTURE_2D, m_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);//GL_RGBA16F
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_tex, 0);
   }
   if (use_tex_wt2)
   {
      if (glIsTexture(m_tex_wt2)!=GL_TRUE)
         glGenTextures(1, &m_tex_wt2);
      //color buffer for current segmented volume
      glBindTexture(GL_TEXTURE_2D, m_tex_wt2);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
            GL_RGBA, GL_FLOAT, NULL);
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_tex_wt2, 0);
   }

   if (m_resize)
   {
      if (use_tex_wt2)
      {
         glBindTexture(GL_TEXTURE_2D, m_tex_wt2);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);
      }
      else
      {
         glBindTexture(GL_TEXTURE_2D, m_tex);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, nx, ny, 0,
               GL_RGBA, GL_FLOAT, NULL);//GL_RGBA16F
      }
   }

   if (TextureRenderer::get_mem_swap() &&
	   TextureRenderer::get_start_update_loop() &&
	   TextureRenderer::get_save_final_buffer())
   {
	   TextureRenderer::reset_save_final_buffer();

	   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_temp);
	   glClearColor(0.0, 0.0, 0.0, 0.0);
	   glClear(GL_COLOR_BUFFER_BIT);
	   glActiveTexture(GL_TEXTURE0);
	   glEnable(GL_TEXTURE_2D);
	   glBindTexture(GL_TEXTURE_2D, m_tex_final);
	   glDisable(GL_BLEND);
	   glDisable(GL_DEPTH_TEST);

	   img_shader =
		   m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
	   if (img_shader)
	   {
		   if (!img_shader->valid())
			   img_shader->create();
		   img_shader->bind();
	   }
	   DrawViewQuad();
	   if (img_shader && img_shader->valid())
		   img_shader->release();

	   glBindTexture(GL_TEXTURE_2D, 0);
	   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   }

   //bind the fbo
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
   if (!TextureRenderer::get_mem_swap() ||
         (TextureRenderer::get_mem_swap() &&
          TextureRenderer::get_clear_chan_buffer()))
   {
      glClearColor(0.0, 0.0, 0.0, 0.0);
      glClear(GL_COLOR_BUFFER_BIT);
      TextureRenderer::reset_clear_chan_buffer();
   }

   //draw multiple volumes at the same time
   m_mvr->draw(m_test_wiref, m_interactive, !m_persp, m_scale_factor, m_intp);
   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (shadow)
   {
	   //draw shadows
	   DrawOLShadows(list, use_tex_wt2?m_tex_wt2:m_tex);
   }

   //bind fbo for final composition
   glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_final);

   if (TextureRenderer::get_mem_swap())
   {
	   //restore m_fbo_temp to m_fbo_final
	   glClearColor(0.0, 0.0, 0.0, 0.0);
	   glClear(GL_COLOR_BUFFER_BIT);
	   glActiveTexture(GL_TEXTURE0);
	   glBindTexture(GL_TEXTURE_2D, m_tex_temp);
	   glDisable(GL_BLEND);
	   glDisable(GL_DEPTH_TEST);

	   img_shader =
		   m_img_shader_factory.shader(IMG_SHADER_TEXTURE_LOOKUP);
	   if (img_shader)
	   {
		   if (!img_shader->valid())
			   img_shader->create();
		   img_shader->bind();
	   }
	   DrawViewQuad();
	   if (img_shader && img_shader->valid())
		   img_shader->release();

	   glBindTexture(GL_TEXTURE_2D, 0);
   }

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, use_tex_wt2?m_tex_wt2:m_tex);
   //build mipmap
   glGenerateMipmap(GL_TEXTURE_2D);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   glEnable(GL_BLEND);
   if (m_vol_method == VOL_METHOD_COMP)
	   glBlendFunc(GL_ONE, GL_ONE);
   else
	   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_DEPTH_TEST);

   //2d adjustment
   img_shader =
      m_img_shader_factory.shader(IMG_SHDR_BRIGHTNESS_CONTRAST_HDR);
   if (img_shader)
   {
      if (!img_shader->valid())
      {
         img_shader->create();
      }
      img_shader->bind();
   }
   Color gamma, brightness, hdr;
   if (m_vol_method == VOL_METHOD_MULTI)
   {
      gamma = m_gamma;
      brightness = m_brightness;
      hdr = m_hdr;
   }
   else
   {
      VolumeData* vd = list[0];
      gamma = vd->GetGamma();
      brightness = vd->GetBrightness();
      hdr = vd->GetHdr();
   }
   img_shader->setLocalParam(0, gamma.r(), gamma.g(), gamma.b(), 1.0);
   img_shader->setLocalParam(1, brightness.r(), brightness.g(), brightness.b(), 1.0);
   img_shader->setLocalParam(2, hdr.r(), hdr.g(), hdr.b(), 0.0);
   //2d adjustment

   DrawViewQuad();

   if (img_shader && img_shader->valid())
      img_shader->release();

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (use_tex_wt2)
   {
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_tex, 0);
	  glBindFramebuffer(GL_FRAMEBUFFER, 0);
   }
}

void VRenderGLView::SetBrush(int mode)
{
   m_prev_focus = FindFocus();
   SetFocus();
   if (m_int_mode == 5 ||
         m_int_mode == 7)
   {
      m_int_mode = 7;
      m_selector.SetMode(1);
   }
   else if (m_int_mode == 8)
   {
      m_selector.SetMode(1);
   }
   else
   {
      m_int_mode = 2;
      m_selector.SetMode(mode);
   }
   m_paint_display = true;
   m_draw_brush = true;
}

void VRenderGLView::UpdateBrushState()
{
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   TreePanel* tree_panel = 0;
   BrushToolDlg* brush_dlg = 0;
   if (vr_frame)
   {
      tree_panel = vr_frame->GetTree();
      brush_dlg = vr_frame->GetBrushToolDlg();
   }

   if(m_key_lock) return;

   if (m_int_mode!=2 && m_int_mode!=7)
   {
      if (wxGetKeyState(WXK_SHIFT))
      {
         SetBrush(2);
         if (tree_panel)
            tree_panel->SelectBrush(TreePanel::ID_BrushAppend);
         if (brush_dlg)
            brush_dlg->SelectBrush(BrushToolDlg::ID_BrushAppend);
         RefreshGLOverlays();
      }
      else if (wxGetKeyState(wxKeyCode('Z')))
      {
         SetBrush(4);
         if (tree_panel)
            tree_panel->SelectBrush(TreePanel::ID_BrushDiffuse);
         if (brush_dlg)
            brush_dlg->SelectBrush(BrushToolDlg::ID_BrushDiffuse);
         RefreshGLOverlays();
      }
      else if (wxGetKeyState(wxKeyCode('X')))
      {
         SetBrush(3);
         if (tree_panel)
            tree_panel->SelectBrush(TreePanel::ID_BrushDesel);
         if (brush_dlg)
            brush_dlg->SelectBrush(BrushToolDlg::ID_BrushDesel);
         RefreshGLOverlays();
      }
   }
   else
   {
      if (m_brush_state)
      {
         if (wxGetKeyState(WXK_SHIFT))
         {
            m_brush_state = 0;
            SetBrush(2);
            if (tree_panel)
               tree_panel->SelectBrush(TreePanel::ID_BrushAppend);
            if (brush_dlg)
               brush_dlg->SelectBrush(BrushToolDlg::ID_BrushAppend);
            RefreshGLOverlays();
         }
         else if (wxGetKeyState(wxKeyCode('Z')))
         {
            m_brush_state = 0;
            SetBrush(4);
            if (tree_panel)
               tree_panel->SelectBrush(TreePanel::ID_BrushDiffuse);
            if (brush_dlg)
               brush_dlg->SelectBrush(BrushToolDlg::ID_BrushDiffuse);
            RefreshGLOverlays();
         }
         else if (wxGetKeyState(wxKeyCode('X')))
         {
            m_brush_state = 0;
            SetBrush(3);
            if (tree_panel)
               tree_panel->SelectBrush(TreePanel::ID_BrushDesel);
            if (brush_dlg)
               brush_dlg->SelectBrush(BrushToolDlg::ID_BrushDesel);
            RefreshGLOverlays();
         }
         else
         {
            SetBrush(m_brush_state);
            RefreshGLOverlays();
         }
      }
      else if (!wxGetKeyState(WXK_SHIFT) &&
            !wxGetKeyState(wxKeyCode('Z')) &&
            !wxGetKeyState(wxKeyCode('X')))
      {
         if (wxGetMouseState().LeftDown())
		 {
 //         Segment();
			m_clear_paint = true;
		 }
         if (m_int_mode == 7)
            m_int_mode = 5;
         else
            m_int_mode = 1;
         m_paint_display = false;
         m_draw_brush = false;
         if (tree_panel)
            tree_panel->SelectBrush(0);
         if (brush_dlg)
            brush_dlg->SelectBrush(0);
         RefreshGLOverlays();

         if (m_prev_focus)
         {
            m_prev_focus->SetFocus();
            m_prev_focus = 0;
         }
      }
   }
}

//selection
void VRenderGLView::Pick()
{
   if (m_draw_all)
   {
      PickVolume();
      PickMesh();
   }
}

void VRenderGLView::PickMesh()
{
	int i;
	int nx = GetSize().x;
	int ny = GetSize().y;
	if (nx<=0 || ny<=0)
		return;
	wxPoint mouse_pos = ScreenToClient(wxGetMousePosition());
	if (mouse_pos.x<0 || mouse_pos.x>=nx ||
		mouse_pos.y<=0 || mouse_pos.y>ny)
		return;

	//projection
	HandleProjection(nx, ny);
	//Transformation
	HandleCamera();
	//obj
	glm::mat4 mv_temp = m_mv_mat;
	//translate object
	m_mv_mat = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	//rotate object
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	//center object
	m_mv_mat = glm::translate(m_mv_mat, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

	//set up fbo
	GLint cur_framebuffer_id;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur_framebuffer_id);
	if (glIsFramebuffer(m_fbo_pick)!=GL_TRUE)
	{
		glGenFramebuffers(1, &m_fbo_pick);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_pick);
		if (glIsTexture(m_tex_pick)!=GL_TRUE)
			glGenTextures(1, &m_tex_pick);
		glBindTexture(GL_TEXTURE_2D, m_tex_pick);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, nx, ny, 0,
			GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, m_tex_pick, 0);
		if (glIsTexture(m_tex_pick_depth)!=GL_TRUE)
			glGenTextures(1, &m_tex_pick_depth);
		glBindTexture(GL_TEXTURE_2D, m_tex_pick_depth);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, nx, ny, 0,
			GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER,
			GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, m_tex_pick_depth, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	if (m_resize)
	{
		glBindTexture(GL_TEXTURE_2D, m_tex_pick);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, nx, ny, 0,
			GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, m_tex_pick_depth);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, nx, ny, 0,
			GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	//bind
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_pick);
	GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glScissor(mouse_pos.x, ny-mouse_pos.y, 1, 1);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	for (i=0; i<(int)m_md_pop_list.size(); i++)
	{
		MeshData* md = m_md_pop_list[i];
		if (md)
		{
			md->SetMatrices(m_mv_mat, m_proj_mat);
			md->DrawInt(i+1);
		}
	}
	glDisable(GL_SCISSOR_TEST);

	unsigned int choose = 0;
	glReadPixels(mouse_pos.x, ny-mouse_pos.y, 1, 1, GL_RED_INTEGER,
		GL_UNSIGNED_INT, (GLvoid*)&choose);
	glBindFramebuffer(GL_FRAMEBUFFER, cur_framebuffer_id);

	if (choose >0 && choose<=(int)m_md_pop_list.size())
	{
		MeshData* md = m_md_pop_list[choose-1];
		if (md)
		{
			VRenderFrame* frame = (VRenderFrame*)m_frame;
			if (frame && frame->GetTree())
			{
				frame->GetTree()->SetFocus();
				frame->GetTree()->Select(m_vrv->GetName(), md->GetName());
			}
		}
	}
	else
	{
		VRenderFrame* frame = (VRenderFrame*)m_frame;
		if (frame && frame->GetCurSelType()==3 && frame->GetTree())
			frame->GetTree()->Select(m_vrv->GetName(), "");
	}
	m_mv_mat = mv_temp;
}

void VRenderGLView::PickVolume()
{
   double dist = 0.0;
   double min_dist = -1.0;
   Point p;
   VolumeData* vd = 0;
   VolumeData* picked_vd = 0;
   for (int i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      vd = m_vd_pop_list[i];
      if (!vd) continue;
      dist = GetPointVolume(p, old_mouse_X, old_mouse_Y, vd, 2, true, 0.5);
      if (dist > 0.0)
      {
         if (min_dist < 0.0)
         {
            min_dist = dist;
            picked_vd = vd;
         }
         else
         {
            if (m_persp)
            {
               if (dist < min_dist)
               {
                  min_dist = dist;
                  picked_vd = vd;
               }
            }
            else
            {
               if (dist > min_dist)
               {
                  min_dist = dist;
                  picked_vd = vd;
               }
            }
         }
      }
   }

   if (picked_vd)
   {
      VRenderFrame* frame = (VRenderFrame*)m_frame;
      if (frame && frame->GetTree())
      {
         frame->GetTree()->SetFocus();
         frame->GetTree()->Select(m_vrv->GetName(), picked_vd->GetName());
      }
   }
}

void VRenderGLView::OnIdle(wxIdleEvent& event)
{
   bool refresh = false;
   bool ref_stat = false;
   bool start_loop = true;
   m_drawing_coord = false;

   //check memory swap status
   if (TextureRenderer::get_mem_swap() &&
         TextureRenderer::get_start_update_loop() &&
         !TextureRenderer::get_done_update_loop())
   {
      refresh = true;
      start_loop = false;
   }

   if (m_capture_rotat ||
         m_capture_tsequ ||
         m_capture_param ||
         m_test_speed)
   {
      refresh = true;
      if (TextureRenderer::get_mem_swap() &&
            TextureRenderer::get_done_update_loop())
         m_pre_draw = true;
   }
   
   wxPoint mouse_pos = wxGetMousePosition();
   wxRect view_reg = GetScreenRect();
   if (view_reg.Contains(mouse_pos))
   {
      UpdateBrushState();
   }

   //draw_mask
   if (wxGetKeyState(wxKeyCode('V')) &&
	   m_draw_mask && !m_key_lock)
   {
      m_draw_mask = false;
      refresh = true;
   }
   if (!wxGetKeyState(wxKeyCode('V')) &&
         !m_draw_mask && !m_key_lock)
   {
      m_draw_mask = true;
      refresh = true;
   }

   //move view
   //left
   if (!m_move_left &&
         wxGetKeyState(WXK_CONTROL) &&
         wxGetKeyState(WXK_LEFT))
   {
      m_move_left = true;

      m_head = Vector(-m_transx, -m_transy, -m_transz);
      m_head.normalize();
      Vector side = Cross(m_up, m_head);
      Vector trans = -(side*(int(0.8*(m_ortho_right-m_ortho_left))));
      m_obj_transx += trans.x();
      m_obj_transy += trans.y();
      m_obj_transz += trans.z();
      //if (m_persp) SetSortBricks();
      refresh = true;
   }
   if (m_move_left &&
         (!wxGetKeyState(WXK_CONTROL) ||
          !wxGetKeyState(WXK_LEFT)))
      m_move_left = false;
   //right
   if (!m_move_right &&
         wxGetKeyState(WXK_CONTROL) &&
         wxGetKeyState(WXK_RIGHT))
   {
      m_move_right = true;

      m_head = Vector(-m_transx, -m_transy, -m_transz);
      m_head.normalize();
      Vector side = Cross(m_up, m_head);
      Vector trans = side*(int(0.8*(m_ortho_right-m_ortho_left)));
      m_obj_transx += trans.x();
      m_obj_transy += trans.y();
      m_obj_transz += trans.z();
      //if (m_persp) SetSortBricks();
      refresh = true;
   }
   if (m_move_right &&
         (!wxGetKeyState(WXK_CONTROL) ||
          !wxGetKeyState(WXK_RIGHT)))
      m_move_right = false;
   //up
   if (!m_move_up &&
         wxGetKeyState(WXK_CONTROL) &&
         wxGetKeyState(WXK_UP))
   {
      m_move_up = true;

      m_head = Vector(-m_transx, -m_transy, -m_transz);
      m_head.normalize();
      Vector trans = -m_up*(int(0.8*(m_ortho_top-m_ortho_bottom)));
      m_obj_transx += trans.x();
      m_obj_transy += trans.y();
      m_obj_transz += trans.z();
      //if (m_persp) SetSortBricks();
      refresh = true;
   }
   if (m_move_up &&
         (!wxGetKeyState(WXK_CONTROL) ||
          !wxGetKeyState(WXK_UP)))
      m_move_up = false;
   //down
   if (!m_move_down &&
         wxGetKeyState(WXK_CONTROL) &&
         wxGetKeyState(WXK_DOWN))
   {
      m_move_down = true;

      m_head = Vector(-m_transx, -m_transy, -m_transz);
      m_head.normalize();
      Vector trans = m_up*(int(0.8*(m_ortho_top-m_ortho_bottom)));
      m_obj_transx += trans.x();
      m_obj_transy += trans.y();
      m_obj_transz += trans.z();
      //if (m_persp) SetSortBricks();
      refresh = true;
   }
   if (m_move_down &&
         (!wxGetKeyState(WXK_CONTROL) ||
          !wxGetKeyState(WXK_DOWN)))
      m_move_down = false;

   //move time sequence
   //forward
   if (!m_tseq_forward &&
         wxGetKeyState(wxKeyCode('d')) && !m_key_lock)
   {
      m_tseq_forward = true;
      Set4DSeqFrame(m_tseq_cur_num+1, true);
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame && vr_frame->GetMovieView())
         vr_frame->GetMovieView()->SetTimeFrame(m_tseq_cur_num);
      refresh = true;
   }
   if (m_tseq_forward &&
         !wxGetKeyState(wxKeyCode('d')) && !m_key_lock)
      m_tseq_forward = false;
   //backforward
   if (!m_tseq_backward &&
         wxGetKeyState(wxKeyCode('a')) && !m_key_lock)
   {
      m_tseq_backward = true;
      Set4DSeqFrame(m_tseq_cur_num-1, true);
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame && vr_frame->GetMovieView())
         vr_frame->GetMovieView()->SetTimeFrame(m_tseq_cur_num);
      refresh = true;
   }
   if (m_tseq_backward &&
         !wxGetKeyState(wxKeyCode('a')) && !m_key_lock)
      m_tseq_backward = false;

   //move clip
   //up
   if (!m_clip_up &&
         wxGetKeyState(wxKeyCode('s')) && !m_key_lock)
   {
      m_clip_up = true;
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame && vr_frame->GetClippingView())
         vr_frame->GetClippingView()->MoveLinkedClippingPlanes(1);
      refresh = true;
   }
   if (m_clip_up &&
         !wxGetKeyState(wxKeyCode('s')) && !m_key_lock)
      m_clip_up = false;
   //down
   if (!m_clip_down &&
         wxGetKeyState(wxKeyCode('w')) && !m_key_lock)
   {
      m_clip_down = true;
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame && vr_frame->GetClippingView())
         vr_frame->GetClippingView()->MoveLinkedClippingPlanes(0);
      refresh = true;
   }
   if (m_clip_down &&
         !wxGetKeyState(wxKeyCode('w')) && !m_key_lock)
      m_clip_down = false;

   if (wxGetKeyState(WXK_ALT) && wxGetKeyState(wxKeyCode('V')) && !m_key_lock)
   {
	   if(m_cur_vol && m_cur_vol->isBrxml()) CopyLevel(m_cur_vol);
   }

   extern CURLM * _g_curlm;
   int handle_count;
   curl_multi_perform(_g_curlm, &handle_count);
   CURLMsg *msg = NULL;
   int remains;
   while( (msg = curl_multi_info_read(_g_curlm, &remains)) != NULL)
   {
	   if(msg->msg == CURLMSG_DONE) RefreshGLOverlays();;
   }

   
//forced refresh //deleted by takashi (for debug)
/*   if (wxGetKeyState(WXK_F5))
   {
      m_updating = true;
      VRenderFrame* frame = (VRenderFrame*)m_frame;
      if (frame && frame->GetStatusBar())
         frame->GetStatusBar()->PushStatusText("Forced Refresh");
      SetFocus();
      Show(false);
      Show(true);
      //SetSortBricks();
      RefreshGL();
      if (frame && frame->GetStatusBar())
         frame->GetStatusBar()->PopStatusText();
   }
   else */if (refresh)
   {
      m_updating = true;
      RefreshGL(ref_stat, start_loop);
   }
}

void VRenderGLView::OnKeyDown(wxKeyEvent& event)
{
   event.Skip();
}

void VRenderGLView::Set3DRotCapture(double start_angle,
      double end_angle,
      double step,
      int frames,
      int rot_axis,
      wxString &cap_file,
      bool rewind,
      int len)
{
   double x, y, z;
   GetRotations(x, y, z);

   m_step = step;
   m_total_frames = frames;
   m_rot_axis = rot_axis;
   m_cap_file = cap_file;
   m_rewind = rewind;
   m_fr_length = len;

   m_movie_seq = 0;
   switch (m_rot_axis)
   {
   case 1: //X
      m_init_angle = x;
      m_cur_angle = x;
      m_start_angle = x + start_angle;
      m_end_angle = x + end_angle;
      break;
   case 2: //Y
      m_init_angle = y;
      m_cur_angle = y;
      m_start_angle = y + start_angle;
      m_end_angle = y + end_angle;
      break;
   case 3: //Z
      m_init_angle = z;
      m_cur_angle = z;
      m_start_angle = z + start_angle;
      m_end_angle = z + end_angle;
      break;
   }

   m_capture = true;
   m_capture_rotat = true;
   m_capture_rotate_over = false;

   if (m_start_angle == m_cur_angle)
      m_stages = 1;
   else
      m_stages = 0;
}

void VRenderGLView::Set3DBatCapture(wxString &cap_file, int begin_frame, int end_frame)
{
   m_cap_file = cap_file;
   m_bat_cur_num = begin_frame;
   m_begin_frame = begin_frame;
   m_end_frame = end_frame;
   m_capture_bat = true;
   m_capture = true;

   if (!m_cap_file.IsEmpty() && m_total_frames>1)
   {
       wxString path = m_cap_file;
#ifdef _WIN32
       wchar_t slash = '\\';
#else
       wchar_t slash = '/';
#endif
      int sep = path.Find(slash, true);
      if (sep != wxNOT_FOUND)
      {
         sep++;
         path = path.Left(sep);
      }

      wxString new_folder = path + m_bat_folder + "_folder";
      CREATE_DIR(new_folder.fn_str());
   }
}

void VRenderGLView::Set4DSeqCapture(wxString &cap_file, int begin_frame, int end_frame, bool rewind, int cur_frame)
{
   m_cap_file = cap_file;
   m_tseq_cur_num = cur_frame > 0 ? cur_frame : begin_frame;
   m_begin_frame = begin_frame;
   m_end_frame = end_frame;
   m_capture_tsequ = true;
   m_capture = true;
   m_movie_seq = begin_frame;
   m_4d_rewind = rewind;
   VRenderFrame* vframe = (VRenderFrame*)m_frame;
   if (vframe && vframe->GetSettingDlg())
      m_run_script = vframe->GetSettingDlg()->GetRunScript();
}

void VRenderGLView::SetParamCapture(wxString &cap_file, int begin_frame, int end_frame, bool rewind)
{
   m_cap_file = cap_file;
   m_param_cur_num = begin_frame;
   m_begin_frame = begin_frame;
   m_end_frame = end_frame;
   m_capture_param = true;
   m_capture = true;
   m_movie_seq = begin_frame;
   m_4d_rewind = rewind;

   wxString s_fr_length = wxString ::Format("%d", end_frame);
   m_fr_length = (int)s_fr_length.length();
}

void VRenderGLView::SetParams(double t)
{
   if (!m_vrv)
      return;
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (!vr_frame)
      return;
   ClippingView* clip_view = vr_frame->GetClippingView();
   Interpolator *interpolator = vr_frame->GetInterpolator();
   if(!interpolator)
      return;
   KeyCode keycode;
   keycode.l0 = 1;
   keycode.l0_name = m_vrv->GetName();

   for (int i=0; i<GetAllVolumeNum(); i++)
   {
      VolumeData* vd = GetAllVolumeData(i);
      if (!vd) continue;

      keycode.l1 = 2;
      keycode.l1_name = vd->GetName();

      //display
      keycode.l2 = 0;
      keycode.l2_name = "display";
      bool bval;
      if (interpolator->GetBoolean(keycode, t, bval))
         vd->SetDisp(bval);

      //clipping planes
      vector<Plane*> *planes = vd->GetVR()->get_planes();
      if (!planes) continue;
      if(planes->size() != 6) continue;
      Plane *plane = 0;
      double val = 0;
      //x1
      plane = (*planes)[0];
      keycode.l2 = 0;
      keycode.l2_name = "x1_val";
      if (interpolator->GetDouble(keycode, t, val))
         plane->ChangePlane(Point(abs(val), 0.0, 0.0),
               Vector(1.0, 0.0, 0.0));
      //x2
      plane = (*planes)[1];
      keycode.l2 = 0;
      keycode.l2_name = "x2_val";
      if (interpolator->GetDouble(keycode, t, val))
         plane->ChangePlane(Point(abs(val), 0.0, 0.0),
               Vector(-1.0, 0.0, 0.0));
      //y1
      plane = (*planes)[2];
      keycode.l2 = 0;
      keycode.l2_name = "y1_val";
      if (interpolator->GetDouble(keycode, t, val))
         plane->ChangePlane(Point(0.0, abs(val), 0.0),
               Vector(0.0, 1.0, 0.0));
      //y2
      plane = (*planes)[3];
      keycode.l2 = 0;
      keycode.l2_name = "y2_val";
      if (interpolator->GetDouble(keycode, t, val))
         plane->ChangePlane(Point(0.0, abs(val), 0.0),
               Vector(0.0, -1.0, 0.0));
      //z1
      plane = (*planes)[4];
      keycode.l2 = 0;
      keycode.l2_name = "z1_val";
      if (interpolator->GetDouble(keycode, t, val))
         plane->ChangePlane(Point(0.0, 0.0, abs(val)),
               Vector(0.0, 0.0, 1.0));
      //z2
      plane = (*planes)[5];
      keycode.l2 = 0;
      keycode.l2_name = "z2_val";
      if (interpolator->GetDouble(keycode, t, val))
         plane->ChangePlane(Point(0.0, 0.0, abs(val)),
               Vector(0.0, 0.0, -1.0));
   }

   bool bx, by, bz;
   //for the view
   keycode.l1 = 1;
   keycode.l1_name = m_vrv->GetName();
   //translation
   double tx, ty, tz;
   keycode.l2 = 0;
   keycode.l2_name = "translation_x";
   bx = interpolator->GetDouble(keycode, t, tx);
   keycode.l2_name = "translation_y";
   by = interpolator->GetDouble(keycode, t, ty);
   keycode.l2_name = "translation_z";
   bz = interpolator->GetDouble(keycode, t, tz);
   if (bx && by && bz)
      SetTranslations(tx, ty, tz);
   //centers
   keycode.l2_name = "center_x";
   bx = interpolator->GetDouble(keycode, t, tx);
   keycode.l2_name = "center_y";
   by = interpolator->GetDouble(keycode, t, ty);
   keycode.l2_name = "center_z";
   bz = interpolator->GetDouble(keycode, t, tz);
   if (bx && by && bz)
      SetCenters(tx, ty, tz);
   //obj translation
   keycode.l2_name = "obj_trans_x";
   bx = interpolator->GetDouble(keycode, t, tx);
   keycode.l2_name = "obj_trans_y";
   by = interpolator->GetDouble(keycode, t, ty);
   keycode.l2_name = "obj_trans_z";
   bz = interpolator->GetDouble(keycode, t, tz);
   if (bx && by && bz)
      SetObjTrans(tx, ty, tz);
   //scale
   double scale;
   keycode.l2_name = "scale";
   if (interpolator->GetDouble(keycode, t, scale))
      m_vrv->SetScaleFactor(scale);
   //rotation
   keycode.l2 = 0;
   keycode.l2_name = "rotation";
   Quaternion q;
   if (interpolator->GetQuaternion(keycode, t, q))
   {
      double rotx, roty, rotz;
      q.ToEuler(rotx, roty, rotz);
      SetRotations(rotx, roty, rotz, true);
   }

   if (clip_view)
      clip_view->SetVolumeData(vr_frame->GetCurSelVol());
   if (vr_frame)
   {
      vr_frame->UpdateTree();
      int index = interpolator->GetKeyIndexFromTime(t);
      vr_frame->GetRecorderDlg()->SetSelection(index);
   }
   SetVolPopDirty();
}

void VRenderGLView::ResetMovieAngle()
{
   double rotx, roty, rotz;
   GetRotations(rotx, roty, rotz);

   switch (m_rot_axis)
   {
   case 1:  //x
      SetRotations(m_init_angle, roty, rotz);
      break;
   case 2:  //y
      SetRotations(rotx, m_init_angle, rotz);
      break;
   case 3:  //z
      SetRotations(rotx, roty, m_init_angle);
      break;
   }
   m_capture = false;
   m_capture_rotat = false;

   RefreshGL();
}

void VRenderGLView::StopMovie()
{
   m_capture = false;
   m_capture_rotat = false;
   m_capture_tsequ = false;
   m_capture_param = false;
}

void VRenderGLView::Get4DSeqFrames(int &start_frame, int &end_frame, int &cur_frame)
{
   for (int i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      VolumeData* vd = m_vd_pop_list[i];
      if (vd && vd->GetReader())
      {
         BaseReader* reader = vd->GetReader();

         int vd_start_frame = 0;
         int vd_end_frame = reader->GetTimeNum()-1;
         int vd_cur_frame = reader->GetCurTime();

         if (i==0)
         {
            //first dataset
            start_frame = vd_start_frame;
            end_frame = vd_end_frame;
            cur_frame = vd_cur_frame;
         }
         else
         {
            //datasets after the first one
            if (vd_end_frame > end_frame)
               end_frame = vd_end_frame;
         }
      }
   }
}

void VRenderGLView::Set4DSeqFrame(int frame, bool run_script)
{
   int start_frame, end_frame, cur_frame;
   Get4DSeqFrames(start_frame, end_frame, cur_frame);
   if (frame > end_frame ||
         frame < start_frame)
      return;

   m_tseq_cur_num = frame;
   VRenderFrame* vframe = (VRenderFrame*)m_frame;
   if (vframe && vframe->GetSettingDlg())
      m_run_script = vframe->GetSettingDlg()->GetRunScript();

   for (int i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      VolumeData* vd = m_vd_pop_list[i];
      if (vd && vd->GetReader())
      {
         BaseReader* reader = vd->GetReader();

		 Texture *tex = vd->GetTexture();
		 if(tex->isBrxml())
		 {
			 BRKXMLReader *br = (BRKXMLReader *)reader;
			 br->SetCurTime(frame);
			 int curlv = tex->GetCurLevel();
			 for (int j = 0; j < br->GetLevelNum(); j++)
			 {
				 tex->setLevel(j);
				 if (vd->GetVR()) vd->GetVR()->clear_brick_buf();
			 }
			 tex->setLevel(curlv);
			 tex->set_FrameAndChannel(frame, vd->GetCurChannel());
			 vd->SetCurTime(reader->GetCurTime());
		 }
		 else
		 {
			 double spcx, spcy, spcz;
			 vd->GetSpacings(spcx, spcy, spcz);

			 Nrrd* data = reader->Convert(frame, vd->GetCurChannel(), false);
			 if (!vd->Replace(data, false))
				 continue;

			 vd->SetCurTime(reader->GetCurTime());
			 vd->SetSpacings(spcx, spcy, spcz);

			 //run script
			 if (m_run_script && run_script)
			 {
#ifdef _WIN32
				 wchar_t slash = '\\';
#else
				 wchar_t slash = '/';
#endif
				 wxString pathname = reader->GetPathName();
				 int pos = pathname.Find(slash, true);
				 wxString scriptname = pathname.Left(pos+1) + "script_4d.txt";
				 if (wxFileExists(scriptname))
				 {
					 m_selector.SetVolume(vd);
					 m_calculator.SetVolumeA(vd);
					 m_cur_vol = vd;
					 Run4DScript(scriptname);
				 }
			 }
		 }
		 //update rulers
		 if (vframe && vframe->GetMeasureDlg())
			vframe->GetMeasureDlg()->UpdateList();

         if (vd->GetVR())
            vd->GetVR()->clear_tex_pool();
      }
   }
   RefreshGL();
}

void VRenderGLView::Get3DBatFrames(int &start_frame, int &end_frame, int &cur_frame)
{
   m_bat_folder = "";

   for (int i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      VolumeData* vd = m_vd_pop_list[i];
      if (vd && vd->GetReader())
      {
         BaseReader* reader = vd->GetReader();
         reader->SetBatch(true);

         int vd_cur_frame = reader->GetCurBatch();
         int vd_start_frame = -vd_cur_frame;
         int vd_end_frame = reader->GetBatchNum()-1-vd_cur_frame;

         if (i > 0)
            m_bat_folder += "_";
         m_bat_folder += wxString(reader->GetDataName());

         if (i==0)
         {
            //first dataset
            start_frame = vd_start_frame;
            end_frame = vd_end_frame;
            cur_frame = 0;
         }
         else
         {
            //datasets after the first one
            if (vd_start_frame < start_frame)
               start_frame = vd_start_frame;
            if (vd_end_frame > end_frame)
               end_frame = vd_end_frame;
         }
      }
   }
   cur_frame -= start_frame;
   end_frame -= start_frame;
   start_frame = 0;
}

void VRenderGLView::Set3DBatFrame(int offset)
{
   int i, j;
   vector<BaseReader*> reader_list;
   m_bat_folder = "";

   for (i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      VolumeData* vd = m_vd_pop_list[i];
      if (vd && vd->GetReader())
      {
         BaseReader* reader = vd->GetReader();
         bool found = false;
         for (j=0; j<(int)reader_list.size(); j++)
         {
            if (reader == reader_list[j])
            {
               found = true;
               break;
            }
         }
         if (!found)
         {
            reader->LoadOffset(offset);
            reader_list.push_back(reader);
         }

         double spcx, spcy, spcz;
         vd->GetSpacings(spcx, spcy, spcz);

         Nrrd* data = reader->Convert(0, vd->GetCurChannel(), false);
         if (vd->Replace(data, true))
            vd->SetDisp(true);
         else
         {
            vd->SetDisp(false);
            continue;
         }

         wxString data_name = wxString(reader->GetDataName());
         if (i > 0)
            m_bat_folder += "_";
         m_bat_folder += data_name;

         int chan_num = 0;
         if (data_name.Find("_1ch")!=-1)
            chan_num = 1;
         else if (data_name.Find("_2ch")!=-1)
            chan_num = 2;
         if (chan_num>0 && vd->GetCurChannel()>=chan_num)
            vd->SetDisp(false);
         else
            vd->SetDisp(true);

         if (reader->GetChanNum() > 1)
            data_name += wxString::Format("_%d", vd->GetCurChannel()+1);
         vd->SetName(data_name);
         vd->SetPath(wxString(reader->GetPathName()));
         vd->SetCurTime(reader->GetCurTime());
         if (!reader->IsSpcInfoValid())
            vd->SetSpacings(spcx, spcy, spcz);
         else
            vd->SetSpacings(reader->GetXSpc(), reader->GetYSpc(), reader->GetZSpc());
         if (vd->GetVR())
            vd->GetVR()->clear_tex_pool();
      }
   }

   InitView(INIT_BOUNDS|INIT_CENTER);

   RefreshGL();

   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (vr_frame)
   {
      vr_frame->UpdateList();
      vr_frame->UpdateTree(vr_frame->GetCurSelVol()->GetName());
   }
}

//pre-draw processings
void VRenderGLView::PreDraw()
{
   //skip if not done with loop
   if (TextureRenderer::get_mem_swap())
   {
      if (m_pre_draw)
         m_pre_draw = false;
      else
         return;
   }

   //if capture a sequence of images
   if (m_capture && (m_capture_tsequ || m_capture_rotat || m_capture_param))
   {
      //if time sequence data are presented
      if (m_capture && m_capture_tsequ)
      {
         int fail_num = 0;

         if (m_tseq_cur_num <= m_end_frame)
         {
            for (int i=0; i<(int)m_vd_pop_list.size(); i++)
            {
               VolumeData* vd = m_vd_pop_list[i];
               if (vd && vd->GetReader())
               {
                  BaseReader* reader = vd->GetReader();

				  wxString s_fr_length = wxString ::Format("%d", reader->GetTimeNum());
                  m_fr_length = (int)s_fr_length.length()>m_fr_length?(int)s_fr_length.length():m_fr_length;

				  Texture *tex = vd->GetTexture();
				  if(tex->isBrxml())
				  {
					  BRKXMLReader *br = (BRKXMLReader *)reader;
					  br->SetCurTime(m_tseq_cur_num);
					  int curlv = tex->GetCurLevel();
					  for (int j = 0; j < br->GetLevelNum(); j++)
					  {
						  tex->setLevel(j);
						  if (vd->GetVR()) vd->GetVR()->clear_brick_buf();
					  }
					  tex->setLevel(curlv);
					  tex->set_FrameAndChannel(m_tseq_cur_num, vd->GetCurChannel());
					  vd->SetCurTime(reader->GetCurTime());
				  }
				  else
				  {

					  double spcx, spcy, spcz;
					  vd->GetSpacings(spcx, spcy, spcz);

					  Nrrd* data = reader->Convert(m_tseq_cur_num, vd->GetCurChannel(), false);
					  if (!vd->Replace(data, false))
					  {
						  fail_num++;
						  continue;
					  }

					  vd->SetCurTime(reader->GetCurTime());
					  vd->SetSpacings(spcx, spcy, spcz);

					  //run script
					  if (m_run_script)
					  {
#ifdef _WIN32
						  wchar_t slash = '\\';
#else
						  wchar_t slash = '/';
#endif
						  wxString pathname = reader->GetPathName();
						  int pos = pathname.Find(slash, true);
						  wxString scriptname = pathname.Left(pos+1) + "script_4d.txt";
						  if (wxFileExists(scriptname))
						  {
							  m_selector.SetVolume(vd);
							  m_calculator.SetVolumeA(vd);
							  m_cur_vol = vd;
							  Run4DScript(scriptname);
						  }
					  }
				  }
				  //update rulers
				  VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
				  if (vr_frame && vr_frame->GetMeasureDlg())
					  vr_frame->GetMeasureDlg()->UpdateList();

				  if (vd->GetVR())
						  vd->GetVR()->clear_tex_pool();
			   }
            }

            VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
            if (vr_frame && vr_frame->GetMovieView())
               vr_frame->GetMovieView()->SetTimeFrame(m_tseq_cur_num);
         }

         if (fail_num == (int)m_vd_pop_list.size() ||
               m_tseq_cur_num > m_end_frame)
         {
            if (m_4d_rewind && !m_run_script)
            {
               m_tseq_cur_num = m_begin_frame;
            }
            else
            {
               m_capture_tsequ = false;
               if (!m_capture_rotat && !m_capture_param)
                  m_capture = false;
            }
         }
         else
            m_tseq_cur_num++;

         if (!m_capture && !m_run_script)
         {
            VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
            if (vr_frame && vr_frame->GetMovieView())
               vr_frame->GetMovieView()->ResetAngle();
         }
      }

      //if rotation movie is set
      if (m_capture && m_capture_rotat)
      {
         if (m_stages==0) //moving to start
         {
            if (fabs(m_cur_angle-m_start_angle) < m_step)
            {
               m_stages = 1;
            }
            else
            {
               m_cur_angle += m_start_angle>m_cur_angle?m_step:-m_step;
               double rotx, roty, rotz;
               GetRotations(rotx, roty, rotz);
               switch (m_rot_axis)
               {
               case 1: //X
                  SetRotations(m_cur_angle, roty, rotz);
                  break;
               case 2: //Y
                  SetRotations(rotx, m_cur_angle, rotz);
                  break;
               case 3: //Z
                  SetRotations(rotx, roty, m_cur_angle);
                  break;
               }
            }

         }
         else if (m_stages==1)
         {
            if (fabs(m_cur_angle-m_end_angle) < m_step)
            {
               if (m_rewind)
                  m_stages = 2;
               else
               {
                  if (m_capture_tsequ)
                     m_stages = 2;
                  else
                     m_capture_rotat = false;
                  m_capture_rotate_over = true;
               }
            }
            else
            {
               m_cur_angle += m_end_angle>m_cur_angle?m_step:-m_step;
               double rotx, roty, rotz;
               GetRotations(rotx, roty, rotz);
               switch (m_rot_axis)
               {
               case 1: //X
                  SetRotations(m_cur_angle, roty, rotz);
                  break;
               case 2: //Y
                  SetRotations(rotx, m_cur_angle, rotz);
                  break;
               case 3: //Z
                  SetRotations(rotx, roty, m_cur_angle);
                  break;
               }
            }
         }
         else if (m_stages==2)
         {
            if (fabs(m_cur_angle-m_init_angle) < m_step)
            {
               if (m_capture_tsequ)
                  m_stages = 0;
               else
                  m_capture_rotat = false;
               m_capture_rotate_over = true;
            }
            else
            {
               m_cur_angle += m_init_angle>m_cur_angle?m_step:-m_step;
               double rotx, roty, rotz;
               GetRotations(rotx, roty, rotz);
               switch (m_rot_axis)
               {
               case 1: //X
                  SetRotations(m_cur_angle, roty, rotz);
                  break;
               case 2: //Y
                  SetRotations(rotx, m_cur_angle, rotz);
                  break;
               case 3: //Z
                  SetRotations(rotx, roty, m_cur_angle);
                  break;
               }
            }
         }
      }

      if (!m_capture_tsequ && m_capture_rotate_over)
      {
         if (!m_capture_bat)
         {
            m_capture = false;
            m_capture_rotat = false;
         }
         else
         {
            if (m_bat_cur_num < m_end_frame)
            {
               Set3DBatFrame(1);
               m_bat_cur_num++;

               //reset
               m_capture = true;
               m_capture_rotat = true;
               m_capture_rotate_over = false;
               m_movie_seq = m_begin_frame;
               m_cur_angle = m_init_angle;
               if (m_start_angle == m_cur_angle)
                  m_stages = 1;
               else
                  m_stages = 0;
               m_cur_angle += m_end_angle>m_cur_angle?m_step:-m_step;
               if (!m_cap_file.IsEmpty() && m_total_frames>1)
               {
                   wxString path = m_cap_file;
#ifdef _WIN32
                   wchar_t slash = '\\';
#else
                   wchar_t slash = '/';
#endif
                  int sep = path.Find(slash, true);
                  if (sep != wxNOT_FOUND)
                  {
                     sep++;
                     path = path.Left(sep);
                  }

                  wxString new_folder = path + m_bat_folder + "_folder";
                  CREATE_DIR(new_folder.fn_str());
               }

               //reset rotation
               double rotx, roty, rotz;
               GetRotations(rotx, roty, rotz);

               switch (m_rot_axis)
               {
               case 1:  //x
                  SetRotations(m_cur_angle, roty, rotz);
                  break;
               case 2:  //y
                  SetRotations(rotx, m_cur_angle, rotz);
                  break;
               case 3:  //z
                  SetRotations(rotx, roty, m_cur_angle);
                  break;
               }

               VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
               if (vr_frame && vr_frame->GetMovieView())
                  vr_frame->GetMovieView()->SetTimeFrame(m_bat_cur_num);
            }
            else
            {
               m_capture = false;
               m_capture_rotat = false;
            }
         }
      }

      //if recording parameters //modified by takashi
      if (m_capture && m_capture_param)
      {
         if (m_param_cur_num <= m_end_frame)
         {
            //change settings
            SetParams(m_param_cur_num);

            m_param_cur_num++;

			if (m_4d_rewind && m_param_cur_num > m_end_frame)m_param_cur_num = m_begin_frame;
         }
		 else
		 {
			 m_capture_param = false;
			 if (!m_capture_rotat && !m_capture_tsequ)
				 m_capture = false;

		 } 
      }
   }

   if (m_manip)
   {
	   if (m_manip_cur_num <= m_manip_end_frame)
	   {
		   SetManipParams(m_manip_cur_num);

		   m_manip_cur_num++;
	   }
	   else
	   {
		   SetManipParams(m_manip_end_frame);
		   EndManipulation();
	   }
   }
}

void VRenderGLView::PostDraw()
{
   //skip if not done with loop
   if (TextureRenderer::get_mem_swap() &&
         TextureRenderer::get_start_update_loop() &&
         !TextureRenderer::get_done_update_loop())
      return;

   //output animations
   if (m_capture && !m_cap_file.IsEmpty())
   {
      wxString outputfilename = m_cap_file;

      if (m_capture_bat && m_total_frames>1)
      {
         wxString path = m_cap_file;
          wxString name = m_cap_file;
#ifdef _WIN32
          wchar_t slash = '\\';
#else
          wchar_t slash = '/';
#endif
         int sep = path.Find(slash, true);
         if (sep != wxNOT_FOUND)
         {
            sep++;
            name = path.Right(path.Length() - sep);
            path = path.Left(sep);
         }

         outputfilename = path + m_bat_folder +
            "_folder\\" + name;
      }

      if (m_capture_rotat || m_capture_tsequ || m_capture_param)
      {
         int i = outputfilename.Find('.', true);
         if (i != wxNOT_FOUND)
         {
            outputfilename.Remove(i);
            wxString seq;
            wxString format;
            format = wxString::Format("_%%0%dd", m_fr_length);
            seq = wxString::Format(format, m_movie_seq);
            if (m_capture_bat && m_total_frames==1)
               seq = wxString::Format(format, m_bat_cur_num);
            outputfilename.Append(seq);
            outputfilename.Append(".tif");
         }
         else
         {
            wxString seq;
            wxString format;
            format = wxString::Format("_%%0%dd", m_fr_length);
            seq = wxString::Format(format, m_movie_seq);
            if (m_capture_bat && m_total_frames==1)
               seq = wxString::Format(format, m_bat_cur_num);
            outputfilename.Append(seq);
            outputfilename.Append(".tif");
         }
         m_movie_seq++;
      }

      //capture
      int x, y, w, h;
      if (m_draw_frame)
      {
         x = m_frame_x;
         y = m_frame_y;
         w = m_frame_w;
         h = m_frame_h;
      }
      else
      {
         x = 0;
         y = 0;
         w = GetSize().x;
         h = GetSize().y;
      }

      int chann = 3; //RGB or RGBA
      glPixelStorei(GL_PACK_ROW_LENGTH, w);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      unsigned char *image = new unsigned char[w*h*chann];
      glReadBuffer(GL_BACK);
      glReadPixels(x, y, w, h, chann==3?GL_RGB:GL_RGBA, GL_UNSIGNED_BYTE, image);
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);
      string str_fn = outputfilename.ToStdString();
      TIFF *out = TIFFOpen(str_fn.c_str(), "wb");
      if (!out)
         return;
      TIFFSetField(out, TIFFTAG_IMAGEWIDTH, w);
      TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
      TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, chann);
      TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
      TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
      TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
      TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
      if (VRenderFrame::GetCompression())
         TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);

      tsize_t linebytes = chann * w;
      unsigned char *buf = NULL;
      buf = (unsigned char *)_TIFFmalloc(linebytes);
      TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, 0));
      for (uint32 row = 0; row < (uint32)h; row++)
      {
         memcpy(buf, &image[(h-row-1)*linebytes], linebytes);// check the index here, and figure out why not using h*linebytes
         if (TIFFWriteScanline(out, buf, row, 0) < 0)
            break;
      }
      TIFFClose(out);
      if (buf)
         _TIFFfree(buf);
      if (image)
         delete []image;

      if (!m_capture_rotat && !m_capture_tsequ && !m_capture_param)
      {
         m_capture = false;
      }
   }
}

//run 4d script
void VRenderGLView::Run4DScript(wxString scriptname)
{
   wxFileInputStream is(scriptname);
   if (!is.IsOk())
      return;
   wxFileConfig fconfig(is);

   int i;
   wxString str;
   wxString pathname;
   wxString format;
   format = wxString::Format("_T%%0%dd", m_fr_length);

   //tasks
   if (fconfig.Exists("/tasks"))
   {
      fconfig.SetPath("/tasks");
      int tasknum = fconfig.Read("tasknum", 0l);
      for (i=0; i<tasknum; i++)
      {
         str = wxString::Format("/tasks/task%d", i);
         if (fconfig.Exists(str))
         {
            fconfig.SetPath(str);
            fconfig.Read("type", &str, "");
            if (str=="noise_reduction")
            {
               double thresh, size;
               fconfig.Read("threshold", &thresh, 0.0);
               fconfig.Read("voxelsize", &size, 0.0);
               int mode;
               fconfig.Read("format", &mode, 0);
               bool bake;
               fconfig.Read("bake", &bake, false);
               bool compression;
               fconfig.Read("compress", &compression, false);
               fconfig.Read("savepath", &pathname, "");
               str = pathname;
                size_t pos = 0;
#ifdef _WIN32
                wxString slash = "\\";
#else
                wxString slash = "/";
#endif
               do
               {
                  pos = pathname.find(slash, pos);
                  if (pos == wxNOT_FOUND)
                     break;
                  pos++;
                  str = pathname.Left(pos);
                  if (!wxDirExists(str))
                     wxMkdir(str);
               } while (true);

               CompAnalysis(0.0, size, thresh, false, false);
               Calculate(6, "", false);
               VolumeData* vd = m_calculator.GetResult();
               if (vd)
               {
                  str = pathname + vd->GetName() +
                     wxString::Format(format, m_tseq_cur_num) + ".tif";
                  vd->Save(str, mode, bake, compression);
                  delete vd;
               }
            }
            else if (str == "selection_tracking")
            {
               //read the size threshold
               int slimit;
               fconfig.Read("size_limit", &slimit, 0);
               //current state:
               //new data has been loaded in system memory
               //old data is in graphics memory
               //old mask in system memory is obsolete
               //new mask is in graphics memory
               //old label is in system memory
               //no label in graphics memory
               //steps:
               int ii, jj, kk;
               int nx, ny, nz;
               //return current mask (into system memory)
               if (!m_cur_vol) break;
               m_cur_vol->GetVR()->return_mask();
               m_cur_vol->GetResolution(nx, ny, nz);
               //find labels in the old that are selected by the current mask
               Texture* tex = m_cur_vol->GetTexture();
               if (!tex) break;
               Nrrd* mask_nrrd = tex->get_nrrd(tex->nmask());
               if (!mask_nrrd) break;
               Nrrd* label_nrrd = tex->get_nrrd(tex->nlabel());
               if(!label_nrrd) break;
               unsigned char* mask_data = (unsigned char*)(mask_nrrd->data);
               if (!mask_data) break;
               unsigned int* label_data = (unsigned int*)(label_nrrd->data);
               if (!label_data) break;
               unordered_map <unsigned int, Lbl>sel_labels;
               unordered_map <unsigned int, Lbl>::iterator label_iter;
               for (ii=0; ii<nx; ii++)
                  for (jj=0; jj<ny; jj++)
                     for (kk=0; kk<nz; kk++)
                     {
                        int index = nx*ny*kk + nx*jj + ii;
                        unsigned int label_value = label_data[index];
                        if (mask_data[index] && label_value)
                        {
                           label_iter = sel_labels.find(label_value);
                           if (label_iter == sel_labels.end())
                           {
                              Lbl lbl;
                              lbl.id = label_value;
                              lbl.size = 1;
                              sel_labels.insert(pair<unsigned int, Lbl>(label_value, lbl));
                           }
                           else
                              label_iter->second.size++;
                        }
                     }
               //load and replace the label
               BaseReader* reader = m_cur_vol->GetReader();
               if (!reader) break;
               wxString data_name = reader->GetCurName(m_tseq_cur_num, m_cur_vol->GetCurChannel());
               wxString label_name = data_name.Left(data_name.find_last_of('.')) + ".lbl";
               LBLReader lbl_reader;
               wstring lblname = label_name.ToStdWstring();
               lbl_reader.SetFile(lblname);
               Nrrd* label_nrrd_new = lbl_reader.Convert(m_tseq_cur_num, m_cur_vol->GetCurChannel(), true);
               if (!label_nrrd_new) break;
               m_cur_vol->LoadLabel(label_nrrd_new);
               label_data = (unsigned int*)(label_nrrd_new->data);
               if (!label_data) break;
               //update the mask according to the new label
               memset((void*)mask_data, 0, sizeof(uint8)*nx*ny*nz);
               for (ii=0; ii<nx; ii++)
                  for (jj=0; jj<ny; jj++)
                     for (kk=0; kk<nz; kk++)
                     {
                        int index = nx*ny*kk + nx*jj + ii;
                        unsigned int label_value = label_data[index];
                        label_iter = sel_labels.find(label_value);
                        if (label_iter != sel_labels.end() &&
                              label_iter->second.size >= (unsigned int)slimit)
                           mask_data[index] = 255;
                     }

               //process trace data
               if (m_trace_group)
               {
                  m_trace_group->SetCurTime(m_tseq_cur_num);
                  unordered_map <unsigned int, Lbl>::iterator id_iter;
                  for (id_iter=sel_labels.begin(); id_iter!=sel_labels.end(); id_iter++)
                     if (id_iter->second.size >= (unsigned int)slimit)
                        m_trace_group->AddID(id_iter->second.id);
               }

               //add traces to trace dialog
               VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
               if (m_vrv && vr_frame && vr_frame->GetTraceDlg())
                  vr_frame->GetTraceDlg()->GetSettings(m_vrv);
            }
            else if (str == "random_colors")
            {
               fconfig.Read("savepath", &pathname, "");
               str = pathname;
                size_t pos = 0;
#ifdef _WIN32
                wxString slash = "\\";
#else
                wxString slash = "/";
#endif
               do
               {
                  pos = pathname.find(slash, pos);
                  if (pos == wxNOT_FOUND)
                     break;
                  pos++;
                  str = pathname.Left(pos);
                  if (!wxDirExists(str))
                     wxMkdir(str);
               } while (true);

               //current state: see selection_tracking
               //steps:
               //load and replace the label
               BaseReader* reader = m_cur_vol->GetReader();
               if (!reader) break;
               wxString data_name = reader->GetCurName(m_tseq_cur_num, m_cur_vol->GetCurChannel());
               wxString label_name = data_name.Left(data_name.find_last_of('.')) + ".lbl";
               LBLReader lbl_reader;
               wstring lblname = label_name.ToStdWstring();
               lbl_reader.SetFile(lblname);
               Nrrd* label_nrrd_new = lbl_reader.Convert(m_tseq_cur_num, m_cur_vol->GetCurChannel(), true);
               if (!label_nrrd_new) break;
               m_cur_vol->LoadLabel(label_nrrd_new);
               //generate RGB volumes
               m_selector.CompExportRandomColor(0, 0, 0, false, false);
               vector<VolumeData*> *vol_list = m_selector.GetResultVols();
               for (int ii=0; ii<(int)vol_list->size(); ii++)
               {
                  VolumeData* vd = (*vol_list)[ii];
                  if (vd)
                  {
                     str = pathname +
                        wxString::Format(format, m_tseq_cur_num) +
                        wxString::Format("_COMP%d", ii+1) + ".tif";
                     vd->Save(str);
                     delete vd;
                  }
               }
            }
            else if (str == "separate_channels")
            {
               int mode;
               fconfig.Read("format", &mode, 0);
               bool bake;
               fconfig.Read("bake", &bake, false);
               bool compression;
               fconfig.Read("compress", &compression, false);
               fconfig.Read("savepath", &pathname, "");
               str = pathname;
                size_t pos = 0;
#ifdef _WIN32
                wxString slash = "\\";
#else
                wxString slash = "/";
#endif
               do
               {
                  pos = pathname.find(slash, pos);
                  if (pos == wxNOT_FOUND)
                     break;
                  pos++;
                  str = pathname.Left(pos);
                  if (!wxDirExists(str))
                     wxMkdir(str);
               } while (true);

               if (wxDirExists(pathname))
               {
                  VolumeData* vd = m_selector.GetVolume();
                  if (vd)
                  {
                     str = pathname + vd->GetName() +
                        wxString::Format(format, m_tseq_cur_num) + ".tif";
                     vd->Save(str, mode, bake, compression);
                  }
               }
            }
         }
      }
   }
}

//draw
void VRenderGLView::OnDraw(wxPaintEvent& event)
{
   Init();
   wxPaintDC dc(this);
   SetCurrent(*m_glRC);

   int nx = GetSize().x;
   int ny = GetSize().y;

   PopMeshList();
   if (m_md_pop_list.size()>0)
      m_draw_type = 2;
   else
      m_draw_type = 1;
 
   PreDraw();
   
   //if (VolumeRenderer::get_done_update_loop()) StartLoopUpdate(); //moved by takashi (to Draw and DrawDP)

   switch (m_draw_type)
   {
   case 1:  //draw volumes only
      Draw();
      break;
   case 2:  //draw volumes and meshes with depth peeling
      DrawDP();
      break;
   }

   if (m_draw_camctr)
      DrawCamCtr();

   if (m_draw_legend)
      DrawLegend();

   if (m_disp_scale_bar)
      DrawScaleBar();

   DrawColormap();

   PostDraw();

   //draw frame after capture
   if (m_draw_frame)
      DrawFrame();

   //draw info
   if (m_draw_info)
      DrawInfo(nx, ny);

   if (m_int_mode == 2 ||
         m_int_mode == 7)  //painting mode
   {
      if (m_draw_brush)
      {
         DrawBrush();
      }
      if (m_paint_enable)
      {
         //paiting mode
         //for volume segmentation
         PaintStroke();
      }
      if (m_paint_enable && m_paint_display)
      {
         //show the paint strokes
         DisplayStroke();
      }
   }
   else if (m_clear_paint)
   {
	   PaintStroke();
   }

   if (m_int_mode == 4)
      m_int_mode = 2;
   if (m_int_mode == 8)
      m_int_mode = 7;

   goTimer->sample();

   SwapBuffers();

   if (m_resize)
      m_resize = false;
}

void VRenderGLView::SetRadius(double r)
{
   m_radius = r;
}

void VRenderGLView::SetCenter()
{
   InitView(INIT_BOUNDS|INIT_CENTER|INIT_OBJ_TRANSL);

   VolumeData *vd = 0;
   if (m_cur_vol)
      vd = m_cur_vol;
   else if (m_vd_pop_list.size())
      vd = m_vd_pop_list[0];

   if (vd)
   {
      BBox bbox = vd->GetBounds();
      VolumeRenderer *vr = vd->GetVR();
      if (!vr) return;
      vector<Plane*> *planes = vr->get_planes();
      if (planes->size() != 6) return;
      double x1, x2, y1, y2, z1, z2;
      double abcd[4];
      (*planes)[0]->get_copy(abcd);
      x1 = fabs(abcd[3])*bbox.max().x();
      (*planes)[1]->get_copy(abcd);
      x2 = fabs(abcd[3])*bbox.max().x();
      (*planes)[2]->get_copy(abcd);
      y1 = fabs(abcd[3])*bbox.max().y();
      (*planes)[3]->get_copy(abcd);
      y2 = fabs(abcd[3])*bbox.max().y();
      (*planes)[4]->get_copy(abcd);
      z1 = fabs(abcd[3])*bbox.max().z();
      (*planes)[5]->get_copy(abcd);
      z2 = fabs(abcd[3])*bbox.max().z();

      m_obj_ctrx = (x1 + x2) / 2.0;
      m_obj_ctry = (y1 + y2) / 2.0;
      m_obj_ctrz = (z1 + z2) / 2.0;

      //SetSortBricks();

      RefreshGL();
   }
}

void VRenderGLView::SetScale121()
{
   //SetCenter();

   int nx = GetSize().x;
   int ny = GetSize().y;
   double aspect = (double)nx / (double)ny;

   double spc_x = 1.0;
   double spc_y = 1.0;
   double spc_z = 1.0;
   VolumeData *vd = 0;
   if (m_cur_vol)
      vd = m_cur_vol;
   else if (m_vd_pop_list.size())
      vd = m_vd_pop_list[0];
   if (vd)
      vd->GetSpacings(spc_x, spc_y, spc_z);
   spc_x = spc_x<EPS?1.0:spc_x;
   spc_y = spc_y<EPS?1.0:spc_y;

   if (aspect > 1.0)
      m_scale_factor = 2.0*m_radius/spc_y/double(ny);
   else
      m_scale_factor = 2.0*m_radius/spc_x/double(nx);

   wxString str = wxString::Format("%.0f", m_scale_factor*100.0);
   m_vrv->m_scale_factor_sldr->SetValue(m_scale_factor*100);
   m_vrv->m_scale_factor_text->ChangeValue(str);

   //SetSortBricks();

   RefreshGL();
}

void VRenderGLView::SetPersp(bool persp)
{
   m_persp = persp;
   if (m_free && !m_persp)
   {
      m_free = false;

      //restore camera translation
      m_transx = m_transx_saved;
      m_transy = m_transy_saved;
      m_transz = m_transz_saved;
      m_ctrx = m_ctrx_saved;
      m_ctry = m_ctry_saved;
      m_ctrz = m_ctrz_saved;
      //restore camera rotation
      m_rotx = m_rotx_saved;
      m_roty = m_roty_saved;
      m_rotz = m_rotz_saved;
      //restore object translation
      m_obj_transx = m_obj_transx_saved;
      m_obj_transy = m_obj_transy_saved;
      m_obj_transz = m_obj_transz_saved;
      //restore scale factor
      m_scale_factor = m_scale_factor_saved;
      wxString str = wxString::Format("%.0f", m_scale_factor*100.0);
      m_vrv->m_scale_factor_sldr->SetValue(m_scale_factor*100);
      m_vrv->m_scale_factor_text->ChangeValue(str);
      m_vrv->m_free_chk->SetValue(false);

      SetRotations(m_rotx, m_roty, m_rotz);
   }
   //SetSortBricks();
}

void VRenderGLView::SetFree(bool free)
{
   m_free = free;
   if (m_vrv->m_free_chk->GetValue() != m_free)
      m_vrv->m_free_chk->SetValue(m_free);
   if (free)
   {
      m_persp = true;
      Vector pos(m_transx, m_transy, m_transz);
      Vector d = pos;
      d.normalize();
      Vector ctr;
      ctr = pos - 0.1*d;
      m_ctrx = ctr.x();
      m_ctry = ctr.y();
      m_ctrz = ctr.z();

      //save camera translation
      m_transx_saved = m_transx;
      m_transy_saved = m_transy;
      m_transz_saved = m_transz;
      m_ctrx_saved = m_ctrx;
      m_ctry_saved = m_ctry;
      m_ctrz_saved = m_ctrz;
      //save camera rotation
      m_rotx_saved = m_rotx;
      m_roty_saved = m_roty;
      m_rotz_saved = m_rotz;
      //save object translateion
      m_obj_transx_saved = m_obj_transx;
      m_obj_transy_saved = m_obj_transy;
      m_obj_transz_saved = m_obj_transz;
      //save scale factor
      m_scale_factor_saved = m_scale_factor;
      m_vrv->m_aov_text->ChangeValue(wxString::Format("%d",int(m_aov)));
      m_vrv->m_aov_sldr->SetValue(m_aov);
   }
   else
   {
      m_ctrx = m_ctry = m_ctrz = 0.0;

      //restore camera translation
      m_transx = m_transx_saved;
      m_transy = m_transy_saved;
      m_transz = m_transz_saved;
      m_ctrx = m_ctrx_saved;
      m_ctry = m_ctry_saved;
      m_ctrz = m_ctrz_saved;
      //restore camera rotation
      m_rotx = m_rotx_saved;
      m_roty = m_roty_saved;
      m_rotz = m_rotz_saved;
      //restore object translation
      m_obj_transx = m_obj_transx_saved;
      m_obj_transy = m_obj_transy_saved;
      m_obj_transz = m_obj_transz_saved;
      //restore scale factor
      m_scale_factor = m_scale_factor_saved;
      wxString str = wxString::Format("%.0f", m_scale_factor*100.0);
      m_vrv->m_scale_factor_sldr->SetValue(m_scale_factor*100);
      m_vrv->m_scale_factor_text->ChangeValue(str);

      SetRotations(m_rotx, m_roty, m_rotz);
   }
   //SetSortBricks();
}

void VRenderGLView::SetAov(double aov)
{
   //view has been changed, sort bricks
   //SetSortBricks();

   m_aov = aov;
   if (m_persp)
   {
      m_vrv->m_aov_text->ChangeValue(wxString::Format("%d", int(m_aov)));
      m_vrv->m_aov_sldr->SetValue(int(m_aov));
   }
}

void VRenderGLView::SetMinPPI(double ppi)
{
   m_min_ppi = ppi;
   m_vrv->m_ppi_text->ChangeValue(wxString::Format("%d", int(m_min_ppi)));
   m_vrv->m_ppi_sldr->SetValue(int(m_min_ppi));
}

void VRenderGLView::SetVolMethod(int method)
{
   //get the volume list m_vd_pop_list
   PopVolumeList();

   m_vol_method = method;

   //ui
   m_vrv->m_volume_seq_rd->SetValue(false);
   m_vrv->m_volume_multi_rd->SetValue(false);
   m_vrv->m_volume_comp_rd->SetValue(false);
   switch (m_vol_method)
   {
   case VOL_METHOD_SEQ:
      m_vrv->m_volume_seq_rd->SetValue(true);
      break;
   case VOL_METHOD_MULTI:
      m_vrv->m_volume_multi_rd->SetValue(true);
      break;
   case VOL_METHOD_COMP:
      m_vrv->m_volume_comp_rd->SetValue(true);
      break;
   }
}

VolumeData* VRenderGLView::GetAllVolumeData(int index)
{
   int cnt = 0;
   int i, j;
   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 2:  //volume data
         if (cnt == index)
            return (VolumeData*)m_layer_list[i];
         cnt++;
         break;
      case 5:  //group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            if (!group)
               break;
            for (j=0; j<group->GetVolumeNum(); j++)
            {
               if (cnt == index)
                  return group->GetVolumeData(j);
               cnt++;
            }
         }
      }
   }
   return 0;
}

VolumeData* VRenderGLView::GetDispVolumeData(int index)
{
   if (GetDispVolumeNum()<=0)
      return 0;

   //get the volume list m_vd_pop_list
   PopVolumeList();

   if (index>=0 && index<(int)m_vd_pop_list.size())
      return m_vd_pop_list[index];
   else
      return 0;
}

MeshData* VRenderGLView::GetMeshData(int index)
{
   if (GetMeshNum()<=0)
      return 0;

   PopMeshList();

   if (index>=0 && index<(int)m_md_pop_list.size())
      return m_md_pop_list[index];
   else
      return 0;
}

VolumeData* VRenderGLView::GetVolumeData(wxString &name)
{
   int i, j;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 2://volume data
         {
            VolumeData* vd = (VolumeData*)m_layer_list[i];
            if (vd && vd->GetName() == name)
               return vd;
         }
         break;
      case 5://group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            if (!group)
               break;
            for (j=0; j<group->GetVolumeNum(); j++)
            {
               VolumeData* vd = group->GetVolumeData(j);
               if (vd && vd->GetName() == name)
                  return vd;
            }
         }
         break;
      }
   }

   return 0;
}

MeshData* VRenderGLView::GetMeshData(wxString &name)
{
   int i, j;

   for (i=0 ; i<(int)m_layer_list.size() ; i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 3://mesh data
         {
            MeshData* md = (MeshData*)m_layer_list[i];
            if (md && md->GetName()== name)
               return md;
         }
         break;
      case 6://mesh group
         {
            MeshGroup* group = (MeshGroup*)m_layer_list[i];
            if (!group) continue;
            for (j=0; j<group->GetMeshNum(); j++)
            {
               MeshData* md = group->GetMeshData(j);
               if (md && md->GetName()==name)
                  return md;
            }
         }
         break;
      }
   }
   return 0;
}

Annotations* VRenderGLView::GetAnnotations(wxString &name)
{
   int i;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 4://annotations
         {
            Annotations* ann = (Annotations*)m_layer_list[i];
            if (ann && ann->GetName()==name)
               return ann;
         }
      }
   }
   return 0;
}

DataGroup* VRenderGLView::GetGroup(wxString &name)
{
   int i;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 5://group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            if (group && group->GetName()==name)
               return group;
         }
      }
   }
   return 0;
}

int VRenderGLView::GetAny()
{
   PopVolumeList();
   PopMeshList();
   return m_vd_pop_list.size() + m_md_pop_list.size();
}

int VRenderGLView::GetDispVolumeNum()
{
   //get the volume list m_vd_pop_list
   PopVolumeList();

   return m_vd_pop_list.size();
}

int VRenderGLView::GetAllVolumeNum()
{
   int num = 0;
   for (int i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 2:  //volume data
         num++;
         break;
      case 5:  //group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            num += group->GetVolumeNum();
         }
         break;
      }
   }
   return num;
}

int VRenderGLView::GetMeshNum()
{
   PopMeshList();
   return m_md_pop_list.size();
}

DataGroup* VRenderGLView::AddVolumeData(VolumeData* vd, wxString group_name)
{
   //m_layer_list.push_back(vd);
   int i;
   DataGroup* group = 0;
   DataGroup* group_temp = 0;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      TreeLayer* layer = m_layer_list[i];
      if (layer && layer->IsA() == 5)
      {
         //layer is group
         group_temp = (DataGroup*) layer;
         if (group_temp && group_temp->GetName()==group_name)
         {
            group = group_temp;
            break;
         }
      }
   }

   if (!group && group_temp)
      group = group_temp;

   if (!group)
   {
      wxString group_name = AddGroup("");
      group = GetGroup(group_name);
      if (!group)
         return 0;
   }
   /*
   for (i=0; i<1; i++)
   {
      VolumeData* vol_data = group->GetVolumeData(i);
      if (vol_data)
      {
         double spcx, spcy, spcz;
         vol_data->GetSpacings(spcx, spcy, spcz);
         vd->SetSpacings(spcx, spcy, spcz);
      }
   }
   */
   group->InsertVolumeData(group->GetVolumeNum()-1, vd);

   if (group && vd)
   {
      Color gamma = group->GetGamma();
      vd->SetGamma(gamma);
      Color brightness = group->GetBrightness();
      vd->SetBrightness(brightness);
      Color hdr = group->GetHdr();
      vd->SetHdr(hdr);
      bool sync_r = group->GetSyncR();
      vd->SetSyncR(sync_r);
      bool sync_g = group->GetSyncG();
      vd->SetSyncG(sync_g);
      bool sync_b = group->GetSyncB();
      vd->SetSyncB(sync_b);

      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame)
      {
         vr_frame->GetAdjustView()->SetVolumeData(vd);
         vr_frame->GetAdjustView()->SetGroupLink(group);
      }
   }

   m_vd_pop_dirty = true;

   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (m_frame)
   {
      AdjustView* adjust_view = vr_frame->GetAdjustView();
      if (adjust_view)
      {
         adjust_view->SetGroupLink(group);
         adjust_view->UpdateSync();
      }
   }
   glHint(GL_POINT_SMOOTH_HINT, GL_DONT_CARE);
   return group;
}

void VRenderGLView::AddMeshData(MeshData* md)
{
   m_layer_list.push_back(md);
   m_md_pop_dirty = true;
}

void VRenderGLView::AddAnnotations(Annotations* ann)
{
   m_layer_list.push_back(ann);
}

void VRenderGLView::ReplaceVolumeData(wxString &name, VolumeData *dst)
{
	int i, j;

	bool found = false;
	DataGroup* group = 0;

	for (i=0; i<(int)m_layer_list.size(); i++)
	{
		if (!m_layer_list[i])
			continue;
		switch (m_layer_list[i]->IsA())
		{
		case 2://volume data
			{
				VolumeData* vd = (VolumeData*)m_layer_list[i];
				if (vd && vd->GetName() == name)
				{
					if (m_cur_vol == vd) m_cur_vol = dst;
					m_layer_list[i] = dst;
					m_vd_pop_dirty = true;
					found = true;
					break;
				}
			}
			break;
		case 5://group
			{
				DataGroup* tmpgroup = (DataGroup*)m_layer_list[i];
				for (j=0; j<tmpgroup->GetVolumeNum(); j++)
				{
					VolumeData* vd = tmpgroup->GetVolumeData(j);
					if (vd && vd->GetName() == name)
					{
						if (m_cur_vol == vd) m_cur_vol = dst;
						tmpgroup->ReplaceVolumeData(j, dst);
						m_vd_pop_dirty = true;
						found = true;
						group = tmpgroup;
						break;
					}
				}
			}
			break;
		}
	}

	if (found)
	{
		VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
		if (vr_frame)
		{
			AdjustView* adjust_view = vr_frame->GetAdjustView();
			if (adjust_view)
			{
				adjust_view->SetVolumeData(dst);
				if (!group) adjust_view->SetGroupLink(group);
				adjust_view->UpdateSync();
			}
		}
	}
}

void VRenderGLView::RemoveVolumeData(wxString &name)
{
   int i, j;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 2://volume data
         {
            VolumeData* vd = (VolumeData*)m_layer_list[i];
            if (vd && vd->GetName() == name)
            {
               m_layer_list.erase(m_layer_list.begin()+i);
               m_vd_pop_dirty = true;
                m_cur_vol = NULL;
               return;
            }
         }
         break;
      case 5://group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            for (j=0; j<group->GetVolumeNum(); j++)
            {
               VolumeData* vd = group->GetVolumeData(j);
               if (vd && vd->GetName() == name)
               {
                  group->RemoveVolumeData(j);
                   m_vd_pop_dirty = true;
                   m_cur_vol = NULL;
                  return;
               }
            }
         }
         break;
      }
   }
}

void VRenderGLView::RemoveMeshData(wxString &name)
{
   int i, j;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 3://mesh data
         {
            MeshData* md = (MeshData*)m_layer_list[i];
            if (md && md->GetName() == name)
            {
               m_layer_list.erase(m_layer_list.begin()+i);
               m_md_pop_dirty = true;
               return;
            }
         }
         break;
      case 6://mesh group
         {
            MeshGroup* group = (MeshGroup*)m_layer_list[i];
            if (!group) continue;
            for (j=0; j<group->GetMeshNum(); j++)
            {
               MeshData* md = group->GetMeshData(j);
               if (md && md->GetName()==name)
               {
                  group->RemoveMeshData(j);
                  m_md_pop_dirty = true;
                  return;
               }
            }
         }
         break;
      }
   }
}

void VRenderGLView::RemoveAnnotations(wxString &name)
{
   for (int i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      if (m_layer_list[i]->IsA() == 4)
      {
         Annotations* ann = (Annotations*)m_layer_list[i];
         if (ann && ann->GetName() == name)
         {
            m_layer_list.erase(m_layer_list.begin()+i);
         }
      }
   }
}

void VRenderGLView::RemoveGroup(wxString &name)
{
   int i, j;
   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 5://group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            if (group && group->GetName() == name)
            {
               for (j=group->GetVolumeNum()-1; j>=0; j--)
               {
                  VolumeData* vd = group->GetVolumeData(j);
                  if (vd)
                  {
                     group->RemoveVolumeData(j);
                     //if add back to view
                  }
               }
               m_layer_list.erase(m_layer_list.begin()+i);
               delete group;
               m_vd_pop_dirty = true;
            }
         }
//      case 6://mesh group
//         {
//            MeshGroup* group = (MeshGroup*)m_layer_list[i];
//            if (group && group->GetName() == name)
//            {
//               for (j=group->GetMeshNum()-1; j>=0; j--)
//               {
//                  MeshData* md = group->GetMeshData(j);
//                  if (md)
//                  {
//                     group->RemoveMeshData(j);
//                  }
//               }
//               m_layer_list.erase(m_layer_list.begin()+i);
//               delete group;
//               m_md_pop_dirty = true;
//            }
//         }
      }
   }
}

//isolate
void VRenderGLView::Isolate(int type, wxString name)
{
   for (int i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i]) continue;

      switch (m_layer_list[i]->IsA())
      {
      case 2://volume
         {
            VolumeData* vd = (VolumeData*)m_layer_list[i];
            if (vd)
            {
               if (type==2 &&
                     vd->GetName() == name)
                  vd->SetDisp(true);
               else
                  vd->SetDisp(false);
            }
         }
         break;
      case 3://mesh
         {
            MeshData* md = (MeshData*)m_layer_list[i];
            if (md)
            {
               if (type==3 &&
                     md->GetName() == name)
                  md->SetDisp(false);
               else
                  md->SetDisp(false);
            }
         }
         break;
      case 4://annotation
         {
            Annotations* ann = (Annotations*)m_layer_list[i];
            if (ann)
            {
               if (type==4 &&
                     ann->GetName() == name)
                  ann->SetDisp(true);
               else
                  ann->SetDisp(false);
            }
         }
         break;
      case 5://volume group
         {
            DataGroup* group = (DataGroup*)m_layer_list[i];
            if (group)
            {
               for (int i=0; i<(int)group->GetVolumeNum(); i++)
               {
                  VolumeData* vd = group->GetVolumeData(i);
                  if (vd)
                  {
                     if (type == 2 &&
                           vd->GetName() == name)
                        vd->SetDisp(true);
                     else
                        vd->SetDisp(false);
                  }
               }
            }
         }
         break;
      case 6://mesh group
         {
            MeshGroup* group = (MeshGroup*)m_layer_list[i];
            if (group)
            {
               for (int i=0; i<(int)group->GetMeshNum(); i++)
               {
                  MeshData* md = group->GetMeshData(i);
                  if (md)
                  {
                     if (type == 3 &&
                           md->GetName() == name)
                        md->SetDisp(true);
                     else
                        md->SetDisp(false);
                  }
               }
            }
         }
         break;
      }
   }
   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move layer of the same level within this view
//source is after the destination
void VRenderGLView::MoveLayerinView(wxString &src_name, wxString &dst_name)
{
   int i, src_index;
   TreeLayer* src = 0;
   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (m_layer_list[i] && m_layer_list[i]->GetName() == src_name)
      {
         src = m_layer_list[i];
         src_index = i;
         m_layer_list.erase(m_layer_list.begin()+i);
         break;
      }
   }
   if (!src)
      return;
   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (m_layer_list[i] && m_layer_list[i]->GetName() == dst_name)
      {
         if (i>=src_index)
            m_layer_list.insert(m_layer_list.begin()+i+1, src);
         else
            m_layer_list.insert(m_layer_list.begin()+i, src);
         break;
      }
   }
   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move layer (volume) of the same level within the given group
//source is after the destination
void VRenderGLView::MoveLayerinGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   DataGroup* group = GetGroup(group_name);
   if (!group)
      return;

   VolumeData* src_vd = 0;
   int i, src_index;
   for (i=0; i<group->GetVolumeNum(); i++)
   {
      wxString name = group->GetVolumeData(i)->GetName();
      if (name == src_name)
      {
         src_index = i;
         src_vd = group->GetVolumeData(i);
         group->RemoveVolumeData(i);
         break;
      }
   }
   if (!src_vd)
      return;
   for (i=0; i<group->GetVolumeNum(); i++)
   {
      wxString name = group->GetVolumeData(i)->GetName();
      if (name == dst_name)
      {
         if (i>=src_index)
            group->InsertVolumeData(i, src_vd);
         else
            group->InsertVolumeData(i-1, src_vd);
         break;
      }
   }
   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move layer (volume) from the given group up one level to this view
//source is after the destination
void VRenderGLView::MoveLayertoView(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   DataGroup* group = GetGroup(group_name);
   if (!group)
      return;

   VolumeData* src_vd = 0;
   int i;
   for (i=0; i<group->GetVolumeNum(); i++)
   {
      wxString name = group->GetVolumeData(i)->GetName();
      if (name == src_name)
      {
         src_vd = group->GetVolumeData(i);
         group->RemoveVolumeData(i);
         break;
      }
   }
   if (!src_vd)
      return;
   if (dst_name == "")
   {
      m_layer_list.push_back(src_vd);
   }
   else
   {
      for (i=0; i<(int)m_layer_list.size(); i++)
      {
         wxString name = m_layer_list[i]->GetName();
         if (name == dst_name)
         {
            m_layer_list.insert(m_layer_list.begin()+i+1, src_vd);
            break;
         }
      }
   }
   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move layer (volume) one level down to the given group
//source is after the destination
void VRenderGLView::MoveLayertoGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   VolumeData* src_vd = 0;
   int i;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      wxString name = m_layer_list[i]->GetName();
      if (name == src_name && m_layer_list[i]->IsA() == 2)//is volume data
      {
         src_vd = (VolumeData*)m_layer_list[i];
         m_layer_list.erase(m_layer_list.begin()+i);
         break;
      }
   }
   DataGroup* group = GetGroup(group_name);
   if (!group || !src_vd)
      return;
   if (group->GetVolumeNum()==0 || dst_name == "")
   {
      group->InsertVolumeData(0, src_vd);
   }
   else
   {
      for (i=0; i<group->GetVolumeNum(); i++)
      {
         wxString name = group->GetVolumeData(i)->GetName();
         if (name == dst_name)
         {
            group->InsertVolumeData(i, src_vd);
            break;
         }
      }
   }

   //set the 2d adjustment settings of the volume the same as the group
   Color gamma = group->GetGamma();
   src_vd->SetGamma(gamma);
   Color brightness = group->GetBrightness();
   src_vd->SetBrightness(brightness);
   Color hdr = group->GetHdr();
   src_vd->SetHdr(hdr);
   bool sync_r = group->GetSyncR();
   src_vd->SetSyncR(sync_r);
   bool sync_g = group->GetSyncG();
   src_vd->SetSyncG(sync_g);
   bool sync_b = group->GetSyncB();
   src_vd->SetSyncB(sync_b);

   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move layer (volume from one group to another different group
//sourece is after the destination
void VRenderGLView::MoveLayerfromtoGroup(wxString &src_group_name, wxString &dst_group_name, wxString &src_name, wxString &dst_name)
{
   DataGroup* src_group = GetGroup(src_group_name);
   if (!src_group)
      return;
   int i;
   VolumeData* src_vd = 0;
   for (i=0; i<src_group->GetVolumeNum(); i++)
   {
      wxString name = src_group->GetVolumeData(i)->GetName();
      if (name == src_name)
      {
         src_vd = src_group->GetVolumeData(i);
         src_group->RemoveVolumeData(i);
         break;
      }
   }
   DataGroup* dst_group = GetGroup(dst_group_name);
   if (!dst_group || !src_vd)
      return;
   if (dst_group->GetVolumeNum()==0 || dst_name == "")
   {
      dst_group->InsertVolumeData(0, src_vd);
   }
   else
   {
      for (i=0; i<dst_group->GetVolumeNum(); i++)
      {
         wxString name = dst_group->GetVolumeData(i)->GetName();
         if (name == dst_name)
         {
            dst_group->InsertVolumeData(i, src_vd);
            break;
         }
      }
   }

   //reset the sync of the source group
   //src_group->ResetSync();

   //set the 2d adjustment settings of the volume the same as the group
   Color gamma = dst_group->GetGamma();
   src_vd->SetGamma(gamma);
   Color brightness = dst_group->GetBrightness();
   src_vd->SetBrightness(brightness);
   Color hdr = dst_group->GetHdr();
   src_vd->SetHdr(hdr);
   bool sync_r = dst_group->GetSyncR();
   src_vd->SetSyncR(sync_r);
   bool sync_g = dst_group->GetSyncG();
   src_vd->SetSyncG(sync_g);
   bool sync_b = dst_group->GetSyncB();
   src_vd->SetSyncB(sync_b);

   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;

   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (m_frame)
   {
      AdjustView* adjust_view = vr_frame->GetAdjustView();
      if (adjust_view)
      {
         adjust_view->SetVolumeData(src_vd);
         adjust_view->SetGroupLink(dst_group);
         adjust_view->UpdateSync();
      }
   }
}

//move mesh within a group
void VRenderGLView::MoveMeshinGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   MeshGroup* group = GetMGroup(group_name);
   if (!group)
      return;

   MeshData* src_md = 0;
   int i, src_index;
   for (i=0; i<group->GetMeshNum(); i++)
   {
      wxString name = group->GetMeshData(i)->GetName();
      if (name == src_name)
      {
         src_index = i;
         src_md = group->GetMeshData(i);
         group->RemoveMeshData(i);
         break;
      }
   }
   if (!src_md)
      return;
   for (i=0; i<group->GetMeshNum(); i++)
   {
      wxString name = group->GetMeshData(i)->GetName();
      if (name == dst_name)
      {
         if (i>=src_index)
            group->InsertMeshData(i, src_md);
         else
            group->InsertMeshData(i-1, src_md);
         break;
      }
   }

   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move mesh out of a group
void VRenderGLView::MoveMeshtoView(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   MeshGroup* group = GetMGroup(group_name);
   if (!group)
      return;

   MeshData* src_md = 0;
   int i;
   for (i=0; i<group->GetMeshNum(); i++)
   {
      wxString name = group->GetMeshData(i)->GetName();
      if (name == src_name)
      {
         src_md = group->GetMeshData(i);
         group->RemoveMeshData(i);
         break;
      }
   }
   if (!src_md)
      return;
   if (dst_name == "")
      m_layer_list.push_back(src_md);
   else
   {
      for (i=0; i<(int)m_layer_list.size(); i++)
      {
         wxString name = m_layer_list[i]->GetName();
         if (name == dst_name)
         {
            m_layer_list.insert(m_layer_list.begin()+i+1, src_md);
            break;
         }
      }
   }
   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move mesh into a group
void VRenderGLView::MoveMeshtoGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   MeshData* src_md = 0;
   int i;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      wxString name = m_layer_list[i]->GetName();
      if (name==src_name && m_layer_list[i]->IsA()==3)
      {
         src_md = (MeshData*)m_layer_list[i];
         m_layer_list.erase(m_layer_list.begin()+i);
         break;
      }
   }
   MeshGroup* group = GetMGroup(group_name);
   if (!group || !src_md)
      return;
   if (group->GetMeshNum()==0 || dst_name=="")
      group->InsertMeshData(0, src_md);
   else
   {
      for (i=0; i<group->GetMeshNum(); i++)
      {
         wxString name = group->GetMeshData(i)->GetName();
         if (name == dst_name)
         {
            group->InsertMeshData(i, src_md);
            break;
         }
      }
   }
   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//move mesh out of then into a group
void VRenderGLView::MoveMeshfromtoGroup(wxString &src_group_name, wxString &dst_group_name, wxString &src_name, wxString &dst_name)
{
   MeshGroup* src_group = GetMGroup(src_group_name);
   if (!src_group)
      return;
   int i;
   MeshData* src_md = 0;
   for (i=0; i<src_group->GetMeshNum(); i++)
   {
      wxString name = src_group->GetMeshData(i)->GetName();
      if (name == src_name)
      {
         src_md = src_group->GetMeshData(i);
         src_group->RemoveMeshData(i);
         break;
      }
   }
   MeshGroup* dst_group = GetMGroup(dst_group_name);
   if (!dst_group || !src_md)
      return;
   if (dst_group->GetMeshNum()==0 ||dst_name=="")
      dst_group->InsertMeshData(0, src_md);
   else
   {
      for (i=0; i<dst_group->GetMeshNum(); i++)
      {
         wxString name = dst_group->GetMeshData(i)->GetName();
         if (name == dst_name)
         {
            dst_group->InsertMeshData(i, src_md);
            break;
         }
      }
   }
   m_vd_pop_dirty = true;
   m_md_pop_dirty = true;
}

//layer control
int VRenderGLView::GetGroupNum()
{
   int group_num = 0;

   for (int i=0; i<(int)m_layer_list.size(); i++)
   {
      TreeLayer *layer = m_layer_list[i];
      if (layer && layer->IsA() == 5)
         group_num++;
   }
   return group_num;
}

int VRenderGLView::GetLayerNum()
{
   return m_layer_list.size();
}

TreeLayer* VRenderGLView::GetLayer(int index)
{
   if (index>=0 && index<(int)m_layer_list.size())
      return m_layer_list[index];
   else
      return 0;
}

wxString VRenderGLView::AddGroup(wxString str, wxString prev_group)
{
   DataGroup* group = new DataGroup();
   if (group && str != "")
      group->SetName(str);

   bool found_prev = false;
   for (int i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 5://group
         {
            DataGroup* group_temp = (DataGroup*)m_layer_list[i];
            if (group_temp && group_temp->GetName()==prev_group)
            {
               m_layer_list.insert(m_layer_list.begin()+i+1, group);
               found_prev = true;
            }
         }
         break;
      }
   }
   if (!found_prev)
      m_layer_list.push_back(group);

   //set default settings
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (vr_frame)
   {
      AdjustView* adjust_view = vr_frame->GetAdjustView();
      if (adjust_view && group)
      {
         Color gamma, brightness, hdr;
         bool sync_r, sync_g, sync_b;
         adjust_view->GetDefaults(gamma, brightness, hdr, sync_r, sync_g, sync_b);
         group->SetGamma(gamma);
         group->SetBrightness(brightness);
         group->SetHdr(hdr);
         group->SetSyncR(sync_r);
         group->SetSyncG(sync_g);
         group->SetSyncB(sync_b);
      }
   }

   if (group)
      return group->GetName();
   else
      return "";
}

wxString VRenderGLView::AddMGroup(wxString str)
{
   MeshGroup* group = new MeshGroup();
   if (group && str != "")
      group->SetName(str);
   m_layer_list.push_back(group);

   if (group)
      return group->GetName();
   else
      return "";
}

MeshGroup* VRenderGLView::GetMGroup(wxString str)
{
   int i;

   for (i=0; i<(int)m_layer_list.size(); i++)
   {
      if (!m_layer_list[i])
         continue;
      switch (m_layer_list[i]->IsA())
      {
      case 6://mesh group
         {
            MeshGroup* group = (MeshGroup*)m_layer_list[i];
            if (group && group->GetName()==str)
               return group;
         }
      }
   }
   return 0;
}

//init
void VRenderGLView::InitView(unsigned int type)
{
   int i;

   if (type&INIT_BOUNDS)
   {
      m_bounds.reset();
      PopVolumeList();
      PopMeshList();

      for (i=0 ; i<(int)m_vd_pop_list.size() ; i++)
         m_bounds.extend(m_vd_pop_list[i]->GetBounds());
      for (i=0 ; i<(int)m_md_pop_list.size() ; i++)
         m_bounds.extend(m_md_pop_list[i]->GetBounds());

      if (m_bounds.valid())
      {
         Vector diag = m_bounds.diagonal();
         m_radius = sqrt(diag.x()*diag.x()+diag.y()*diag.y()) / 2.0;
         if (m_radius<0.1)
            m_radius = 5.0;
         m_near_clip = m_radius / 1000.0;
         m_far_clip = m_radius * 100.0;
      }
   }

   if (type&INIT_CENTER)
   {
      if (m_bounds.valid())
      {
         m_obj_ctrx = (m_bounds.min().x() + m_bounds.max().x())/2.0;
         m_obj_ctry = (m_bounds.min().y() + m_bounds.max().y())/2.0;
         m_obj_ctrz = (m_bounds.min().z() + m_bounds.max().z())/2.0;
      }
   }

   if (type&INIT_TRANSL/*||!m_init_view*/)
   {
      m_distance = m_radius/tan(d2r(m_aov/2.0));
      m_init_dist = m_distance;
      m_transx = 0.0;
      m_transy = 0.0;
      m_transz = m_distance;
      if (!m_vrv->m_use_dft_settings)
         m_scale_factor = 1.0;
   }

   if (type&INIT_OBJ_TRANSL)
   {
      m_obj_transx = 0.0;
      m_obj_transy = 0.0;
      m_obj_transz = 0.0;
   }

   if (type&INIT_ROTATE||!m_init_view)
   {
      if (!m_vrv->m_use_dft_settings)
      {
         m_q = Quaternion(0,0,0,1);
         m_q.ToEuler(m_rotx, m_roty, m_rotz);
      }
   }

   m_init_view = true;

}

void VRenderGLView::DrawBounds()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	vector<float> vertex;
	vertex.reserve(16*3);

	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.min().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.min().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.min().z());
	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.min().z());

	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.max().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.max().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.max().z());
	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.max().z());

	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.min().z());
	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.max().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.min().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.min().y()); vertex.push_back(m_bounds.max().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.min().z());
	vertex.push_back(m_bounds.max().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.max().z());
	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.min().z());
	vertex.push_back(m_bounds.min().x()); vertex.push_back(m_bounds.max().y()); vertex.push_back(m_bounds.max().z());

	ShaderProgram* shader =
		m_img_shader_factory.shader(IMG_SHDR_DRAW_GEOMETRY);
	if (shader)
	{
		if (!shader->valid())
			shader->create();
		shader->bind();
	}
	shader->setLocalParam(0, 1.0, 1.0, 1.0, 1.0);
	glm::mat4 matrix = m_proj_mat * m_mv_mat;
	shader->setLocalParamMatrix(0, glm::value_ptr(matrix));

	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex.size(), &vertex[0], GL_DYNAMIC_DRAW);
	glBindVertexArray(m_misc_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (const GLvoid*)0);
	glDrawArrays(GL_LINE_LOOP, 0, 4);
	glDrawArrays(GL_LINE_LOOP, 4, 4);
	glDrawArrays(GL_LINES, 8, 8);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	if (shader && shader->valid())
		shader->release();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
}

void VRenderGLView::DrawClippingPlanes(bool border, int face_winding)
{
   int i;
	bool link = false;
	VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
	if (vr_frame && vr_frame->GetClippingView())
		link = vr_frame->GetClippingView()->GetChannLink();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	if (face_winding == FRONT_FACE)
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
	}
	else if (face_winding == BACK_FACE)
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
	}
	else if (face_winding == CULL_OFF)
		glDisable(GL_CULL_FACE);

	glCullFace(GL_BACK);

	ShaderProgram* shader =
		m_img_shader_factory.shader(IMG_SHDR_DRAW_GEOMETRY);
	if (shader)
	{
		if (!shader->valid())
			shader->create();
		shader->bind();
	}

	for (i=0; i<GetDispVolumeNum(); i++)
	{
		VolumeData* vd = GetDispVolumeData(i);
		if (!vd)
			continue;

		if (!link && vd!=m_cur_vol)
			continue;

		VolumeRenderer *vr = vd->GetVR();
		if (!vr)
			continue;

		vector<Plane*> *planes = vr->get_planes();
		if (planes->size() != 6)
			continue;

		//calculating planes
		//get six planes
		Plane* px1 = (*planes)[0];
		Plane* px2 = (*planes)[1];
		Plane* py1 = (*planes)[2];
		Plane* py2 = (*planes)[3];
		Plane* pz1 = (*planes)[4];
		Plane* pz2 = (*planes)[5];

		//calculate 4 lines
		Vector lv_x1z1, lv_x1z2, lv_x2z1, lv_x2z2;
		Point lp_x1z1, lp_x1z2, lp_x2z1, lp_x2z2;
		//x1z1
		if (!px1->Intersect(*pz1, lp_x1z1, lv_x1z1))
			continue;
		//x1z2
		if (!px1->Intersect(*pz2, lp_x1z2, lv_x1z2))
			continue;
		//x2z1
		if (!px2->Intersect(*pz1, lp_x2z1, lv_x2z1))
			continue;
		//x2z2
		if (!px2->Intersect(*pz2, lp_x2z2, lv_x2z2))
			continue;

		//calculate 8 points
//		Point p0, p1, p2, p3, p4, p5, p6, p7;
		Point pp[8];
		//p0 = l_x1z1 * py1
		if (!py1->Intersect(lp_x1z1, lv_x1z1, pp[0]))
			continue;
		//p1 = l_x1z2 * py1
		if (!py1->Intersect(lp_x1z2, lv_x1z2, pp[1]))
			continue;
		//p2 = l_x2z1 *py1
		if (!py1->Intersect(lp_x2z1, lv_x2z1, pp[2]))
			continue;
		//p3 = l_x2z2 * py1
		if (!py1->Intersect(lp_x2z2, lv_x2z2, pp[3]))
			continue;
		//p4 = l_x1z1 * py2
		if (!py2->Intersect(lp_x1z1, lv_x1z1, pp[4]))
			continue;
		//p5 = l_x1z2 * py2
		if (!py2->Intersect(lp_x1z2, lv_x1z2, pp[5]))
			continue;
		//p6 = l_x2z1 * py2
		if (!py2->Intersect(lp_x2z1, lv_x2z1, pp[6]))
			continue;
		//p7 = l_x2z2 * py2
		if (!py2->Intersect(lp_x2z2, lv_x2z2, pp[7]))
			continue;

		//draw the six planes out of the eight points
		//get color
		Color color(1.0, 1.0, 1.0);
		double plane_trans = 0.0;
		if (face_winding == BACK_FACE &&
			(m_clip_mask == 3 ||
			m_clip_mask == 12 ||
			m_clip_mask == 48 ||
			m_clip_mask == 1 ||
			m_clip_mask == 2 ||
			m_clip_mask == 4 ||
			m_clip_mask == 8 ||
			m_clip_mask == 16 ||
			m_clip_mask == 32 ||
			m_clip_mask == 64)
			)
			plane_trans = 0.3;

		if (face_winding == FRONT_FACE)
		{
			plane_trans = 0.3;
		}

		if (!link)
		{
			color = vd->GetColor();
		}

		//transform
		if (!vd->GetTexture())
			continue;
		Transform *tform = vd->GetTexture()->transform();
		if (!tform)
			continue;
		double mvmat[16];
		tform->get_trans(mvmat);
		double sclx, scly, sclz;
		vd->GetScalings(sclx, scly, sclz);
		glm::mat4 mv_mat = glm::scale(m_mv_mat,
			glm::vec3(float(sclx), float(scly), float(sclz)));
		glm::mat4 mv_mat2 = glm::mat4(
			mvmat[0], mvmat[4], mvmat[8], mvmat[12],
			mvmat[1], mvmat[5], mvmat[9], mvmat[13],
			mvmat[2], mvmat[6], mvmat[10], mvmat[14],
			mvmat[3], mvmat[7], mvmat[11], mvmat[15]);
		mv_mat = mv_mat * mv_mat2;
		glm::mat4 matrix = m_proj_mat * mv_mat;
		shader->setLocalParamMatrix(0, glm::value_ptr(matrix));

		vector<float> vertex;
		vertex.reserve(8*3);
		vector<uint32_t> index;
		index.reserve(6*4*2);

		//vertices
		for (size_t pi=0; pi<8; ++pi)
		{
			vertex.push_back(pp[pi].x());
			vertex.push_back(pp[pi].y());
			vertex.push_back(pp[pi].z());
		}
		//indices
		index.push_back(4); index.push_back(0); index.push_back(5); index.push_back(1);
		index.push_back(4); index.push_back(0); index.push_back(1); index.push_back(5);
		index.push_back(7); index.push_back(3); index.push_back(6); index.push_back(2);
		index.push_back(7); index.push_back(3); index.push_back(2); index.push_back(6);
		index.push_back(1); index.push_back(0); index.push_back(3); index.push_back(2);
		index.push_back(1); index.push_back(0); index.push_back(2); index.push_back(3);
		index.push_back(4); index.push_back(5); index.push_back(6); index.push_back(7);
		index.push_back(4); index.push_back(5); index.push_back(7); index.push_back(6);
		index.push_back(0); index.push_back(4); index.push_back(2); index.push_back(6);
		index.push_back(0); index.push_back(4); index.push_back(6); index.push_back(2);
		index.push_back(5); index.push_back(1); index.push_back(7); index.push_back(3);
		index.push_back(5); index.push_back(1); index.push_back(3); index.push_back(7);

		glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex.size(), &vertex[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_misc_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*index.size(), &index[0], GL_DYNAMIC_DRAW);

		glBindVertexArray(m_misc_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (const GLvoid*)0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_misc_ibo);

		//draw
		//x1 = (p4, p0, p1, p5)
		if (m_clip_mask & 1)
		{
			shader->setLocalParam(0, 1.0, 0.5, 0.5, plane_trans);
			glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, (const GLvoid*)0);
			if (border)
			{
				shader->setLocalParam(0, color.r(), color.g(), color.b(), plane_trans);
				glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (const GLvoid*)(4*4));
			}
		}
		//x2 = (p7, p3, p2, p6)
		if (m_clip_mask & 2)
		{
			shader->setLocalParam(0, 1.0, 0.5, 1.0, plane_trans);
			glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, (const GLvoid*)(8*4));
			if (border)
			{
				shader->setLocalParam(0, color.r(), color.g(), color.b(), plane_trans);
				glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (const GLvoid*)(12*4));
			}
		}
		//y1 = (p1, p0, p2, p3)
		if (m_clip_mask & 4)
		{
			shader->setLocalParam(0, 0.5, 1.0, 0.5, plane_trans);
			glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, (const GLvoid*)(16*4));
			if (border)
			{
				shader->setLocalParam(0, color.r(), color.g(), color.b(), plane_trans);
				glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (const GLvoid*)(20*4));
			}
		}
		//y2 = (p4, p5, p7, p6)
		if (m_clip_mask & 8)
		{
			shader->setLocalParam(0, 1.0, 1.0, 0.5, plane_trans);
			glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, (const GLvoid*)(24*4));
			if (border)
			{
				shader->setLocalParam(0, color.r(), color.g(), color.b(), plane_trans);
				glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (const GLvoid*)(28*4));
			}
		}
		//z1 = (p0, p4, p6, p2)
		if (m_clip_mask & 16)
		{
			shader->setLocalParam(0, 0.5, 0.5, 1.0, plane_trans);
			glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, (const GLvoid*)(32*4));
			if (border)
			{
				shader->setLocalParam(0, color.r(), color.g(), color.b(), plane_trans);
				glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (const GLvoid*)(36*4));
			}
		}
		//z2 = (p5, p1, p3, p7)
		if (m_clip_mask & 32)
		{
			shader->setLocalParam(0, 0.5, 1.0, 1.0, plane_trans);
			glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, (const GLvoid*)(40*4));
			if (border)
			{
				shader->setLocalParam(0, color.r(), color.g(), color.b(), plane_trans);
				glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (const GLvoid*)(44*4));
			}
		}

		glDisableVertexAttribArray(0);
		//unbind
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	if (shader && shader->valid())
		shader->release();

	glFrontFace(GL_CCW);
}

void VRenderGLView::DrawGrid()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	size_t grid_num = 5;
	size_t line_num = grid_num*2 + 1;
	size_t i;
	vector<float> vertex;
	vertex.reserve(line_num*4*3);

	double gap = m_distance / grid_num;
	for (i=0; i<line_num; ++i)
	{
		vertex.push_back(float(-m_distance+gap*i));
		vertex.push_back(float(0.0));
		vertex.push_back(float(-m_distance));
		vertex.push_back(float(-m_distance+gap*i));
		vertex.push_back(float(0.0));
		vertex.push_back(float(m_distance));
	}
	for (i=0; i<line_num; ++i)
	{
		vertex.push_back(float(-m_distance));
		vertex.push_back(float(0.0));
		vertex.push_back(float(-m_distance+gap*i));
		vertex.push_back(float(m_distance));
		vertex.push_back(float(0.0));
		vertex.push_back(float(-m_distance+gap*i));
	}

	ShaderProgram* shader =
		m_img_shader_factory.shader(IMG_SHDR_DRAW_GEOMETRY);
	if (shader)
	{
		if (!shader->valid())
			shader->create();
		shader->bind();
	}
	shader->setLocalParam(0, m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b(), 1.0);
	glm::mat4 matrix = m_proj_mat * m_mv_mat;
	shader->setLocalParamMatrix(0, glm::value_ptr(matrix));

	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex.size(), &vertex[0], GL_DYNAMIC_DRAW);
	glBindVertexArray(m_misc_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (const GLvoid*)0);
	glDrawArrays(GL_LINES, 0, line_num*4);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	if (shader && shader->valid())
		shader->release();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
}

void VRenderGLView::DrawCamCtr()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	double len;
	if (m_camctr_size > 0.0)
		len = m_distance*tan(d2r(m_aov/2.0))*m_camctr_size/10.0;
	else
		len = fabs(m_camctr_size);

	vector<float> vertex;
	vertex.reserve(6*3);

	vertex.push_back(0.0); vertex.push_back(0.0); vertex.push_back(0.0);
	vertex.push_back(len); vertex.push_back(0.0); vertex.push_back(0.0);
	vertex.push_back(0.0); vertex.push_back(0.0); vertex.push_back(0.0);
	vertex.push_back(0.0); vertex.push_back(len); vertex.push_back(0.0);
	vertex.push_back(0.0); vertex.push_back(0.0); vertex.push_back(0.0);
	vertex.push_back(0.0); vertex.push_back(0.0); vertex.push_back(len);

	ShaderProgram* shader =
		m_img_shader_factory.shader(IMG_SHDR_DRAW_GEOMETRY);
	if (shader)
	{
		if (!shader->valid())
			shader->create();
		shader->bind();
	}
	glm::mat4 matrix = m_proj_mat * m_mv_mat;
	shader->setLocalParamMatrix(0, glm::value_ptr(matrix));

	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex.size(), &vertex[0], GL_DYNAMIC_DRAW);
	glBindVertexArray(m_misc_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (const GLvoid*)0);

	shader->setLocalParam(0, 1.0, 0.0, 0.0, 1.0);
	glDrawArrays(GL_LINES, 0, 2);
	shader->setLocalParam(0, 0.0, 1.0, 0.0, 1.0);
	glDrawArrays(GL_LINES, 2, 2);
	shader->setLocalParam(0, 0.0, 0.0, 1.0, 1.0);
	glDrawArrays(GL_LINES, 4, 2);

	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	if (shader && shader->valid())
		shader->release();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
}

void VRenderGLView::DrawFrame()
{
	int nx, ny;
	nx = GetSize().x;
	ny = GetSize().y;
	glm::mat4 proj_mat = glm::ortho(float(0), float(nx), float(0), float(ny));

	glDisable(GL_DEPTH_TEST);
	GLfloat line_width = 1.0f;

	vector<float> vertex;
	vertex.reserve(4*3);

	vertex.push_back(m_frame_x-1); vertex.push_back(m_frame_y-1); vertex.push_back(0.0);
	vertex.push_back(m_frame_x+m_frame_w+1); vertex.push_back(m_frame_y-1); vertex.push_back(0.0);
	vertex.push_back(m_frame_x+m_frame_w+1); vertex.push_back(m_frame_y+m_frame_h+1); vertex.push_back(0.0);
	vertex.push_back(m_frame_x-1); vertex.push_back(m_frame_y+m_frame_h+1); vertex.push_back(0.0);

	ShaderProgram* shader =
		m_img_shader_factory.shader(IMG_SHDR_DRAW_GEOMETRY);
	if (shader)
	{
		if (!shader->valid())
			shader->create();
		shader->bind();
	}
	shader->setLocalParam(0, 1.0, 1.0, 0.0, 1.0);
	shader->setLocalParamMatrix(0, glm::value_ptr(proj_mat));

	//draw frame
	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex.size(), &vertex[0], GL_DYNAMIC_DRAW);
	glBindVertexArray(m_misc_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (const GLvoid*)0);
	glDrawArrays(GL_LINE_LOOP, 0, 4);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	if (shader && shader->valid())
		shader->release();

	glEnable(GL_DEPTH_TEST);
}

void VRenderGLView::DrawScaleBar()
{
   double offset = 0.0;
   if (m_draw_legend)
      offset = m_sb_height;

   BitmapFontType font = BITMAP_FONT_TYPE_HELVETICA_12;
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (vr_frame && vr_frame->GetSettingDlg())
      font = (BitmapFontType)vr_frame->GetSettingDlg()->GetTextFont();

   int nx = GetSize().x;
   int ny = GetSize().y;

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();
   gluOrtho2D(0.0, 1.0, 0.0, 1.0);
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   glPushAttrib(GL_ENABLE_BIT);
   glDisable(GL_LIGHTING);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_DEPTH_TEST);

   GLfloat line_width;
   glGetFloatv(GL_LINE_WIDTH, &line_width);
   glLineWidth(5.0);
   //text color
   glColor3d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b());

   double len = m_sb_length / (m_ortho_right-m_ortho_left);
   double textlen = renderTextLen(font, m_sb_text.To8BitData().data());
   if (m_draw_frame)
   {
      glBegin(GL_LINE_STRIP);
      glVertex2d((0.95*m_frame_w+m_frame_x)/nx, (0.05*m_frame_h+m_frame_y+offset)/ny);
      glVertex2d((0.95*m_frame_w+m_frame_x)/nx-len, (0.05*m_frame_h+m_frame_y+offset)/ny);
      glEnd();
      if (m_disp_scale_bar_text)
      {
         beginRenderText(nx, ny);
         renderText(
               0.95*m_frame_w+m_frame_x-len*nx/2.0-textlen/2.0,
               ny-0.065*m_frame_h-m_frame_y-offset,
               font,
               m_sb_text.To8BitData().data());
         endRenderText();
      }
   }
   else
   {
      glBegin(GL_LINE_STRIP);
      glVertex2d(0.95, 0.05+offset/ny);
      glVertex2d(0.95-len, 0.05+offset/ny);
      glEnd();
      if (m_disp_scale_bar_text)
      {
         beginRenderText(nx, ny);
         renderText(
               nx*(0.95-len/2.0)-textlen/2.0,
               0.935*ny-offset,
               font,
               m_sb_text.To8BitData().data());
         endRenderText();
      }
   }

   glLineWidth(line_width);
   glPopAttrib();

   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
   glMatrixMode(GL_MODELVIEW);
}

void VRenderGLView::DrawLegend()
{
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (!vr_frame)
      return;

   BitmapFontType font = BITMAP_FONT_TYPE_HELVETICA_12;
   double font_height = 17.0;
   if (vr_frame->GetSettingDlg())
   {
      font = (BitmapFontType)vr_frame->GetSettingDlg()->GetTextFont();
      switch (font)
      {
      case BITMAP_FONT_TYPE_8_BY_13:
         font_height = 18.0;
         break;
      case BITMAP_FONT_TYPE_9_BY_15:
         font_height = 20.0;
         break;
      case BITMAP_FONT_TYPE_HELVETICA_10:
         font_height = 10.0;
         break;
      case BITMAP_FONT_TYPE_HELVETICA_12:
         font_height = 17.0;
         break;
      case BITMAP_FONT_TYPE_HELVETICA_18:
         font_height = 23.0;
         break;
      case BITMAP_FONT_TYPE_TIMES_ROMAN_10:
         font_height = 15.0;
         break;
      case BITMAP_FONT_TYPE_TIMES_ROMAN_24:
         font_height = 29.0;
         break;
      default:
         font = BITMAP_FONT_TYPE_HELVETICA_12;
         font_height = 17.0;
         break;
      }
   }

   int nx = GetSize().x;
   int ny = GetSize().y;

   double xoffset = 10.0;
   double yoffset = 10.0;
   if (m_draw_frame)
   {
      xoffset = 10.0+m_frame_x;
      yoffset = ny-m_frame_h-m_frame_y+10.0;
   }

   wxString wxstr;
   double length = 0.0;
   double name_len = 0.0;
   double gap_width = 15.0;
   int lines = 0;
   int i;
   //first pass
   for (i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      if (m_vd_pop_list[i] && m_vd_pop_list[i]->GetLegend())
      {
         wxstr = m_vd_pop_list[i]->GetName();
         name_len = renderTextLen(font, wxstr.To8BitData().data())+font_height;
         length += name_len;
         if (length < double(m_draw_frame?m_frame_w:nx)-gap_width)
         {
            length += gap_width;
         }
         else
         {
            length = name_len + gap_width;
            lines++;
         }
      }
   }
   for (i=0; i<(int)m_md_pop_list.size(); i++)
   {
      if (m_md_pop_list[i] && m_md_pop_list[i]->GetLegend())
      {
         wxstr = m_md_pop_list[i]->GetName();
         name_len = renderTextLen(font, wxstr.To8BitData().data())+font_height;
         length += name_len;
         if (length < double(m_draw_frame?m_frame_w:nx)-gap_width)
         {
            length += gap_width;
         }
         else
         {
            length = name_len + gap_width;
            lines++;
         }
      }
   }

   //second pass
   int cur_line = 0;
   double xpos;
   length = 0.0;
   for (i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      if (m_vd_pop_list[i] && m_vd_pop_list[i]->GetLegend())
      {
         wxstr = m_vd_pop_list[i]->GetName();
         xpos = length;
         name_len = renderTextLen(font, wxstr.To8BitData().data())+font_height;
         length += name_len;
         if (length < double(m_draw_frame?m_frame_w:nx)-gap_width)
         {
            length += gap_width;
         }
         else
         {
            length = name_len + gap_width;
            xpos = 0.0;
            cur_line++;
         }
         bool highlighted = false;
         if (vr_frame->GetCurSelType() == 2 &&
               vr_frame->GetCurSelVol() &&
               vr_frame->GetCurSelVol()->GetName() == wxstr)
            highlighted = true;
         DrawName(xpos+xoffset, ny-(lines-cur_line+0.1)*font_height-yoffset,
               nx, ny, wxstr, m_vd_pop_list[i]->GetColor(),
               font, font_height, highlighted);
      }
   }
   for (i=0; i<(int)m_md_pop_list.size(); i++)
   {
      if (m_md_pop_list[i] && m_md_pop_list[i]->GetLegend())
      {
         wxstr = m_md_pop_list[i]->GetName();
         xpos = length;
         name_len = renderTextLen(font, wxstr.To8BitData().data())+font_height;
         length += name_len;
         if (length < double(m_draw_frame?m_frame_w:nx)-gap_width)
         {
            length += gap_width;
         }
         else
         {
            length = name_len + gap_width;
            xpos = 0.0;
            cur_line++;
         }
         Color amb, diff, spec;
         double shine, alpha;
         m_md_pop_list[i]->GetMaterial(amb, diff, spec, shine, alpha);
         Color c(diff.r(), diff.g(), diff.b());
         bool highlighted = false;
         if (vr_frame->GetCurSelType() == 3 &&
               vr_frame->GetCurSelMesh() &&
               vr_frame->GetCurSelMesh()->GetName() == wxstr)
            highlighted = true;
         DrawName(xpos+xoffset, ny-(lines-cur_line+0.1)*font_height-yoffset,
               nx, ny, wxstr, c, font, font_height, highlighted);
      }
   }

   m_sb_height = (lines+1)*font_height;
}

void VRenderGLView::DrawName(double x, double y, int nx, int ny,
      wxString name, Color color,
      BitmapFontType font, double font_height,
      bool highlighted)
{
   //text color
   beginRenderText(nx, ny);
   glColor4d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b(), 1.0);
   glBegin(GL_QUADS);
   glVertex2d(x+0.2*font_height-1.0, y-0.2*font_height+1.0);
   glVertex2d(x+0.8*font_height+1.0, y-0.2*font_height+1.0);
   glVertex2d(x+0.8*font_height+1.0, y-0.8*font_height-1.0);
   glVertex2d(x+0.2*font_height-1.0, y-0.8*font_height-1.0);
   glEnd();
   glColor4d(color.r(), color.g(), color.b(), 1.0);
   glBegin(GL_QUADS);
   glVertex2d(x+0.2*font_height, y-0.2*font_height);
   glVertex2d(x+0.8*font_height, y-0.2*font_height);
   glVertex2d(x+0.8*font_height, y-0.8*font_height);
   glVertex2d(x+0.2*font_height, y-0.8*font_height);
   glEnd();
   glColor4d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b(), 1.0);
   renderText(x+font_height, y-0.25*font_height, font, name.To8BitData().data());
   if (highlighted)
   {
      glColor4d(color.r(), color.g(), color.b(), 1.0);
      renderText(x+font_height-1.0, y-0.25*font_height-1.0, font, name.To8BitData().data());
   }
   endRenderText();
}

void VRenderGLView::DrawGradBg()
{
   glm::mat4 proj_mat = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	Color color1, color2;
	HSVColor hsv_color1(m_bg_color);
	if (hsv_color1.val() > 0.5)
	{
		if (hsv_color1.sat() > 0.3)
		{
			color1 = Color(HSVColor(hsv_color1.hue(),
				hsv_color1.sat() * 0.1,
				Min(hsv_color1.val() + 0.3, 1.0)));
			color2 = Color(HSVColor(hsv_color1.hue(),
				hsv_color1.sat() * 0.3,
				Min(hsv_color1.val() + 0.1, 1.0)));
		}
		else
		{
			color1 = Color(HSVColor(hsv_color1.hue(),
				hsv_color1.sat() * 0.1,
				Max(hsv_color1.val() - 0.5, 0.0)));
			color2 = Color(HSVColor(hsv_color1.hue(),
				hsv_color1.sat() * 0.3,
				Max(hsv_color1.val() - 0.3, 0.0)));
		}
	}
	else
	{
		color1 = Color(HSVColor(hsv_color1.hue(),
			hsv_color1.sat() * 0.1,
			Min(hsv_color1.val() + 0.7, 1.0)));
		color2 = Color(HSVColor(hsv_color1.hue(),
			hsv_color1.sat() * 0.3,
			Min(hsv_color1.val() + 0.5, 1.0)));
	}

	vector<float> vertex;
	vertex.reserve(16*3);
	vertex.push_back(0.0); vertex.push_back(0.0); vertex.push_back(0.0);
	vertex.push_back(m_bg_color.r()); vertex.push_back(m_bg_color.g()); vertex.push_back(m_bg_color.b());
	vertex.push_back(1.0); vertex.push_back(0.0); vertex.push_back(0.0);
	vertex.push_back(m_bg_color.r()); vertex.push_back(m_bg_color.g()); vertex.push_back(m_bg_color.b());
	vertex.push_back(0.0); vertex.push_back(0.3); vertex.push_back(0.0);
	vertex.push_back(color1.r()); vertex.push_back(color1.g()); vertex.push_back(color1.b());
	vertex.push_back(1.0); vertex.push_back(0.3); vertex.push_back(0.0);
	vertex.push_back(color1.r()); vertex.push_back(color1.g()); vertex.push_back(color1.b());
	vertex.push_back(0.0); vertex.push_back(0.5); vertex.push_back(0.0);
	vertex.push_back(color2.r()); vertex.push_back(color2.g()); vertex.push_back(color2.b());
	vertex.push_back(1.0); vertex.push_back(0.5); vertex.push_back(0.0);
	vertex.push_back(color2.r()); vertex.push_back(color2.g()); vertex.push_back(color2.b());
	vertex.push_back(0.0); vertex.push_back(1.0); vertex.push_back(0.0);
	vertex.push_back(m_bg_color.r()); vertex.push_back(m_bg_color.g()); vertex.push_back(m_bg_color.b());
	vertex.push_back(1.0); vertex.push_back(1.0); vertex.push_back(0.0);
	vertex.push_back(m_bg_color.r()); vertex.push_back(m_bg_color.g()); vertex.push_back(m_bg_color.b());

	ShaderProgram* shader =
		m_img_shader_factory.shader(IMG_SHDR_DRAW_GEOMETRY_COLOR3);
	if (shader)
	{
		if (!shader->valid())
			shader->create();
		shader->bind();
	}
	shader->setLocalParamMatrix(0, glm::value_ptr(proj_mat));

	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex.size(), &vertex[0], GL_DYNAMIC_DRAW);
	glBindVertexArray(m_misc_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_misc_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (const GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (const GLvoid*)12);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 8);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	if (shader && shader->valid())
		shader->release();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
}

void VRenderGLView::DrawColormap()
{
   bool draw = false;

   int num = 0;
   int vd_index;
   double max_val = 255.0;
   bool enable_alpha = false;

   for (int i=0; i<GetDispVolumeNum(); i++)
   {
      VolumeData* vd = GetDispVolumeData(i);
      if (vd && vd->GetColormapMode() == 1 && vd->GetDisp())
      {
         num++;
         vd_index = i;
      }
   }

   if (num == 0)
      return;
   else if (num == 1)
   {
      VolumeData* vd_view = GetDispVolumeData(vd_index);
      if (vd_view)
      {
         draw = true;
         double low, high;
         vd_view->GetColormapValues(low, high);
         m_value_2 = low;
         m_value_6 = high;
         m_value_4 = (low+high)/2.0;
         m_value_3 = (low+m_value_4)/2.0;
         m_value_5 = (m_value_4+high)/2.0;
         max_val = vd_view->GetMaxValue();
         enable_alpha = vd_view->GetEnableAlpha();
      }
   }
   else if (num > 1)
   {
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame)
      {
         VolumeData* vd = vr_frame->GetCurSelVol();
         if (vd && vd->GetDisp())
         {
            wxString str = vd->GetName();
            VolumeData* vd_view = GetVolumeData(str);
            if (vd_view && vd_view->GetColormapDisp())
            {
               draw = true;
               double low, high;
               vd_view->GetColormapValues(low, high);
               m_value_2 = low;
               m_value_6 = high;
               m_value_4 = (low+high)/2.0;
               m_value_3 = (low+m_value_4)/2.0;
               m_value_5 = (m_value_4+high)/2.0;
               max_val = vd_view->GetMaxValue();
               enable_alpha = vd_view->GetEnableAlpha();
            }
         }
      }
   }

   if (!draw)
      return;

   double offset = 0.0;
   if (m_draw_legend)
      offset = m_sb_height;

   int nx = GetSize().x;
   int ny = GetSize().y;

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();
   gluOrtho2D(0.0, 1.0, 0.0, 1.0);
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   glPushAttrib(GL_ENABLE_BIT);
   glDisable(GL_LIGHTING);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_DEPTH_TEST);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   GLfloat line_width;
   glGetFloatv(GL_LINE_WIDTH, &line_width);
   glLineWidth(1.0);

   //draw colormap
   if (m_draw_frame)
   {
      glBegin(GL_QUAD_STRIP);
      //color 1
      glColor4d(m_color_1.r(), m_color_1.g(), m_color_1.b(), enable_alpha?0.0:1.0);
      glVertex2d((0.01*m_frame_w+m_frame_x)/nx, (0.1*m_frame_h+m_frame_y+offset)/ny);
      glVertex2d((0.05*m_frame_w+m_frame_x)/nx, (0.1*m_frame_h+m_frame_y+offset)/ny);
      //color 2
      glColor4d(m_color_2.r(), m_color_2.g(), m_color_2.b(), enable_alpha?m_value_2:1.0);
      glVertex2d((0.01*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_2)*m_frame_h+m_frame_y+offset)/ny);
      glVertex2d((0.05*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_2)*m_frame_h+m_frame_y+offset)/ny);
      //color 3
      glColor4d(m_color_3.r(), m_color_3.g(), m_color_3.b(), enable_alpha?m_value_3:1.0);
      glVertex2d((0.01*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_3)*m_frame_h+m_frame_y+offset)/ny);
      glVertex2d((0.05*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_3)*m_frame_h+m_frame_y+offset)/ny);
      //color 4
      glColor4d(m_color_4.r(), m_color_4.g(), m_color_4.b(), enable_alpha?m_value_4:1.0);
      glVertex2d((0.01*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_4)*m_frame_h+m_frame_y+offset)/ny);
      glVertex2d((0.05*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_4)*m_frame_h+m_frame_y+offset)/ny);
      //color 5
      glColor4d(m_color_5.r(), m_color_5.g(), m_color_5.b(), enable_alpha?m_value_5:1.0);
      glVertex2d((0.01*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_5)*m_frame_h+m_frame_y+offset)/ny);
      glVertex2d((0.05*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_5)*m_frame_h+m_frame_y+offset)/ny);
      //color 6
      glColor4d(m_color_6.r(), m_color_6.g(), m_color_6.b(), enable_alpha?m_value_6:1.0);
      glVertex2f((0.01*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_6)*m_frame_h+m_frame_y+offset)/ny);
      glVertex2f((0.05*m_frame_w+m_frame_x)/nx, ((0.1+0.4*m_value_6)*m_frame_h+m_frame_y+offset)/ny);
      //color 7
      glColor4d(m_color_7.r(), m_color_7.g(), m_color_7.b(), 1.0);
      glVertex2d((0.01*m_frame_w+m_frame_x)/nx, (0.5*m_frame_h+m_frame_y+offset)/ny);
      glVertex2d((0.05*m_frame_w+m_frame_x)/nx, (0.5*m_frame_h+m_frame_y+offset)/ny);
      glEnd();

      char str[32];
      //text color
      glColor3d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b());
      //value 1
      SPRINTF(str, 32, "%d", 0);
      beginRenderText(nx, ny);
      renderText(0.052*m_frame_w+m_frame_x, ny-0.1*m_frame_h-m_frame_y-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 2
      SPRINTF(str, 32, "%d", int(m_value_2*max_val));
      beginRenderText(nx, ny);
      renderText(0.052*m_frame_w+m_frame_x, ny-(0.1+0.4*m_value_2)*m_frame_h-m_frame_y-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 4
      SPRINTF(str, 32, "%d", int(m_value_4*max_val));
      beginRenderText(nx, ny);
      renderText(0.052*m_frame_w+m_frame_x, ny-(0.1+0.4*m_value_4)*m_frame_h-m_frame_y-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 6
      SPRINTF(str, 32, "%d", int(m_value_6*max_val));
      beginRenderText(nx, ny);
      renderText(0.052*m_frame_w+m_frame_x, ny-(0.1+0.4*m_value_6)*m_frame_h-m_frame_y-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 7
      SPRINTF(str, 32, "%d", int(max_val));
      beginRenderText(nx, ny);
      renderText(0.052*m_frame_w+m_frame_x, ny-0.5*m_frame_h-m_frame_y-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
   }
   else
   {
      glBegin(GL_QUAD_STRIP);
      //color 1
      glColor4d(m_color_1.r(), m_color_1.g(), m_color_1.b(), enable_alpha?0.0:1.0);
      glVertex2d(0.01, 0.1+offset/ny);
      glVertex2d(0.05, 0.1+offset/ny);
      //color 2
      glColor4d(m_color_2.r(), m_color_2.g(), m_color_2.b(), enable_alpha?m_value_2:1.0);
      glVertex2d(0.01, 0.1+0.4*m_value_2+offset/ny);
      glVertex2d(0.05, 0.1+0.4*m_value_2+offset/ny);
      //color 3
      glColor4d(m_color_3.r(), m_color_3.g(), m_color_3.b(), enable_alpha?m_value_3:1.0);
      glVertex2d(0.01, 0.1+0.4*m_value_3+offset/ny);
      glVertex2d(0.05, 0.1+0.4*m_value_3+offset/ny);
      //color 4
      glColor4d(m_color_4.r(), m_color_4.g(), m_color_4.b(), enable_alpha?m_value_4:1.0);
      glVertex2d(0.01, 0.1+0.4*m_value_4+offset/ny);
      glVertex2d(0.05, 0.1+0.4*m_value_4+offset/ny);
      //color 5
      glColor4d(m_color_5.r(), m_color_5.g(), m_color_5.b(), enable_alpha?m_value_5:1.0);
      glVertex2d(0.01, 0.1+0.4*m_value_5+offset/ny);
      glVertex2d(0.05, 0.1+0.4*m_value_5+offset/ny);
      //color 6
      glColor4d(m_color_6.r(), m_color_6.g(), m_color_6.b(), enable_alpha?m_value_6:1.0);
      glVertex2d(0.01, 0.1+0.4*m_value_6+offset/ny);
      glVertex2d(0.05, 0.1+0.4*m_value_6+offset/ny);
      //color 7
      glColor4d(m_color_7.r(), m_color_7.g(), m_color_7.b(), 1.0);
      glVertex2d(0.01, 0.5+offset/ny);
      glVertex2d(0.05, 0.5+offset/ny);
      glEnd();

      char str[32];
      //text color
      glColor3d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b());
      //value 1
      SPRINTF(str, 32, "%d", 0);
      beginRenderText(nx, ny);
      renderText(0.052*nx, 0.9*ny-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 2
      SPRINTF(str, 32, "%d", int(m_value_2*max_val));
      beginRenderText(nx, ny);
      renderText(0.052*nx, (0.9-0.4*m_value_2)*ny-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 4
      SPRINTF(str, 32, "%d", int(m_value_4*max_val));
      beginRenderText(nx, ny);
      renderText(0.052*nx, (0.9-0.4*m_value_4)*ny-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 6
      SPRINTF(str, 32, "%d", int(m_value_6*max_val));
      beginRenderText(nx, ny);
      renderText(0.052*nx, (0.9-0.4*m_value_6)*ny-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
      //value 7
      SPRINTF(str, 32, "%d", int(max_val));
      beginRenderText(nx, ny);
      renderText(0.052*nx, 0.5*ny-offset, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
   }

   glLineWidth(line_width);
   glPopAttrib();

   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
   glMatrixMode(GL_MODELVIEW);
}

void VRenderGLView::DrawInfo(int nx, int ny)
{
   //text color
   glColor3d(m_bg_color_inv.r(), m_bg_color_inv.g(), m_bg_color_inv.b());

   double fps_ = 1.0/goTimer->average();
   char str[1024];
   if (TextureRenderer::get_mem_swap())
   {
	  SPRINTF(str, 128, 
            "FPS: %.2f, Bricks: %d, Quota: %d, Int: %s, Time: %lu",
            fps_>=0.0&&fps_<300.0?fps_:0.0,
            TextureRenderer::get_total_brick_num(),
            TextureRenderer::get_quota_bricks(),
            m_interactive?"Yes":"No",
            TextureRenderer::get_cor_up_time());

      ////budget_test
      //if (m_interactive)
      //  tos <<
      //  TextureRenderer::get_quota_bricks()
      //  << "\t" <<
      //  TextureRenderer::get_finished_bricks()
      //  << "\t" <<
      //  TextureRenderer::get_queue_last()
      //  << "\t" <<
      //  int(TextureRenderer::get_finished_bricks()*
      //    TextureRenderer::get_up_time()/
      //    TextureRenderer::get_consumed_time())
      //  << "\n";
   }
   else
      SPRINTF(str, 128, "FPS: %.2f", fps_>=0.0&&fps_<300.0?fps_:0.0);
   beginRenderText(nx, ny);
   renderText(10, 20, BITMAP_FONT_TYPE_HELVETICA_12, str);
   endRenderText();
/*
   float *mvmat = glm::value_ptr(m_mv_mat);
   SPRINTF(str, 1024, "%.5f %.5f %.5f %.5f", mvmat[0],mvmat[1],mvmat[2],mvmat[3]);
   beginRenderText(nx, ny);
   renderText(10, 60, BITMAP_FONT_TYPE_HELVETICA_12, str);
   endRenderText();
   SPRINTF(str, 1024, "%.5f %.5f %.5f %.5f", mvmat[4],mvmat[5],mvmat[6],mvmat[7]);
   beginRenderText(nx, ny);
   renderText(10, 80, BITMAP_FONT_TYPE_HELVETICA_12, str);
   endRenderText();
   SPRINTF(str, 1024, "%.5f %.5f %.5f %.5f", mvmat[8],mvmat[9],mvmat[10],mvmat[11]);
   beginRenderText(nx, ny);
   renderText(10, 100, BITMAP_FONT_TYPE_HELVETICA_12, str);
   endRenderText();
   SPRINTF(str, 1024, "%.5f %.5f %.5f %.5f", mvmat[12],mvmat[13],mvmat[14],mvmat[15]);
   beginRenderText(nx, ny);
   renderText(10, 120, BITMAP_FONT_TYPE_HELVETICA_12, str);
   endRenderText();
*/
   if (m_draw_coord)
   {
      Point p;
      wxPoint mouse_pos = ScreenToClient(wxGetMousePosition());
      if ((m_cur_vol && GetPointVolumeBox(p, mouse_pos.x, mouse_pos.y, m_cur_vol)>0.0) ||
            GetPointPlane(p, mouse_pos.x, mouse_pos.y)>0.0)
      {
         SPRINTF(str, 128, "T: %d  X: %.2f  Y: %.2f  Z: %.2f", m_tseq_cur_num, p.x(), p.y(), p.z());
         beginRenderText(nx, ny);
         renderText(10, 40, BITMAP_FONT_TYPE_HELVETICA_12, str);
         endRenderText();
      }
   }
   else
   {
      SPRINTF(str, 128, "T: %d", m_tseq_cur_num);
      beginRenderText(nx, ny);
      renderText(10, 40, BITMAP_FONT_TYPE_HELVETICA_12, str);
      endRenderText();
   }

   if (m_test_wiref)
   {
      if (m_vol_method == VOL_METHOD_MULTI && m_mvr)
      {
         SPRINTF(str, 128, "SLICES: %d", m_mvr->get_slice_num());
         beginRenderText(nx, ny);
         renderText(10, 35, BITMAP_FONT_TYPE_HELVETICA_12, str);
         endRenderText();
      }
      else
      {
         for (int i=0; i<(int)m_vd_pop_list.size(); i++)
         {
            VolumeData* vd = m_vd_pop_list[i];
            if (vd && vd->GetVR())
            {
               SPRINTF(str, 128, "SLICES_%d: %d", i+1, vd->GetVR()->get_slice_num());
               beginRenderText(nx, ny);
               renderText(10, 35+15*i, BITMAP_FONT_TYPE_HELVETICA_12, str);
               endRenderText();
            }
         }
      }
   }
}

Quaternion VRenderGLView::Trackball(int p1x, int p1y, int p2x, int p2y)
{
   Quaternion q;
   Vector a; /* Axis of rotation */
   double phi;  /* how much to rotate about axis */

   if (p1x == p2x && p1y == p2y)
   {
      /* Zero rotation */
      return q;
   }

   if (m_rot_lock)
   {
      if (abs(p2x-p1x)<50 &&
            abs(p2y-p1y)<50)
         return q;
   }

   a = Vector(p1y-p2y, p2x-p1x, 0.0);
   phi = a.length()/3.0;
   a.normalize();
   Quaternion q_a(a);
   //rotate back to local
   Quaternion q_a2 = (-m_q) * q_a * m_q;
   a = Vector(q_a2.x, q_a2.y, q_a2.z);
   a.normalize();

   q = Quaternion(phi, a);
   q.Normalize();

   if (m_rot_lock)
   {
      double rotx, roty, rotz;
      q.ToEuler(rotx, roty, rotz);
      rotx = int(rotx/45.0)*45.0;
      roty = int(roty/45.0)*45.0;
      rotz = int(rotz/45.0)*45.0;
      q.FromEuler(rotx, roty, rotz);
   }

   return q;
}

Quaternion VRenderGLView::TrackballClip(int p1x, int p1y, int p2x, int p2y)
{
   Quaternion q;
   Vector a; /* Axis of rotation */
   double phi;  /* how much to rotate about axis */

   if (p1x == p2x && p1y == p2y)
   {
      /* Zero rotation */
      return q;
   }

   a = Vector(p2y-p1y, p2x-p1x, 0.0);
   phi = a.length()/3.0;
   a.normalize();
   Quaternion q_a(a);
   //rotate back to local
   Quaternion q2;
   q2.FromEuler(-m_rotx, m_roty, m_rotz);
   q_a = (q2) * q_a * (-q2);
   q_a = (m_q_cl) * q_a * (-m_q_cl);
   a = Vector(q_a.x, q_a.y, q_a.z);
   a.normalize();

   q = Quaternion(phi, a);
   q.Normalize();

   return q;
}

void VRenderGLView::Q2A()
{
   //view changed, re-sort bricks
   //SetSortBricks();

   m_q.ToEuler(m_rotx, m_roty, m_rotz);

   if (m_roty>360.0)
      m_roty -= 360.0;
   if (m_roty<0.0)
      m_roty += 360.0;
   if (m_rotx>360.0)
      m_rotx -= 360.0;
   if (m_rotx<0.0)
      m_rotx += 360.0;
   if (m_rotz>360.0)
      m_rotz -= 360.0;
   if (m_rotz<0.0)
      m_rotz += 360.0;

   if (m_clip_mode)
   {
      if (m_clip_mode == 1)
         m_q_cl.FromEuler(m_rotx, -m_roty, -m_rotz);

      vector<Plane*> *planes = 0;
      for (int i=0; i<(int)m_vd_pop_list.size(); i++)
      {
         if (!m_vd_pop_list[i])
            continue;

         double spcx, spcy, spcz;
         int resx, resy, resz;
         m_vd_pop_list[i]->GetSpacings(spcx, spcy, spcz);
         m_vd_pop_list[i]->GetResolution(resx, resy, resz);
         Vector scale;
         if (spcx>0.0 && spcy>0.0 && spcz>0.0)
         {
            scale = Vector(1.0/resx/spcx, 1.0/resy/spcy, 1.0/resz/spcz);
            scale.safe_normalize();
         }
         else
            scale = Vector(1.0, 1.0, 1.0);

         if (m_vd_pop_list[i]->GetVR())
            planes = m_vd_pop_list[i]->GetVR()->get_planes();
         if (planes && planes->size()==6)
         {
            double x1, x2, y1, y2, z1, z2;
            double abcd[4];
            (*planes)[0]->get_copy(abcd);
            x1 = fabs(abcd[3]);
            (*planes)[1]->get_copy(abcd);
            x2 = fabs(abcd[3]);
            (*planes)[2]->get_copy(abcd);
            y1 = fabs(abcd[3]);
            (*planes)[3]->get_copy(abcd);
            y2 = fabs(abcd[3]);
            (*planes)[4]->get_copy(abcd);
            z1 = fabs(abcd[3]);
            (*planes)[5]->get_copy(abcd);
            z2 = fabs(abcd[3]);

            Vector trans1(-0.5, -0.5, -0.5);
            Vector trans2(0.5, 0.5, 0.5);

            (*planes)[0]->Restore();
            (*planes)[0]->Translate(trans1);
            (*planes)[0]->Rotate(m_q_cl);
            (*planes)[0]->Scale(scale);
            (*planes)[0]->Translate(trans2);

            (*planes)[1]->Restore();
            (*planes)[1]->Translate(trans1);
            (*planes)[1]->Rotate(m_q_cl);
            (*planes)[1]->Scale(scale);
            (*planes)[1]->Translate(trans2);

            (*planes)[2]->Restore();
            (*planes)[2]->Translate(trans1);
            (*planes)[2]->Rotate(m_q_cl);
            (*planes)[2]->Scale(scale);
            (*planes)[2]->Translate(trans2);

            (*planes)[3]->Restore();
            (*planes)[3]->Translate(trans1);
            (*planes)[3]->Rotate(m_q_cl);
            (*planes)[3]->Scale(scale);
            (*planes)[3]->Translate(trans2);

            (*planes)[4]->Restore();
            (*planes)[4]->Translate(trans1);
            (*planes)[4]->Rotate(m_q_cl);
            (*planes)[4]->Scale(scale);
            (*planes)[4]->Translate(trans2);

            (*planes)[5]->Restore();
            (*planes)[5]->Translate(trans1);
            (*planes)[5]->Rotate(m_q_cl);
            (*planes)[5]->Scale(scale);
            (*planes)[5]->Translate(trans2);
         }
      }
   }
}

void VRenderGLView::A2Q()
{
   //view changed, re-sort bricks
   //SetSortBricks();

   m_q.FromEuler(m_rotx, m_roty, m_rotz);

   if (m_clip_mode)
   {
      if (m_clip_mode == 1)
         m_q_cl.FromEuler(m_rotx, -m_roty, -m_rotz);

      for (int i=0; i<(int)m_vd_pop_list.size(); i++)
      {
         if (!m_vd_pop_list[i])
            continue;

         vector<Plane*> *planes = 0;
         double spcx, spcy, spcz;
         int resx, resy, resz;
         m_vd_pop_list[i]->GetSpacings(spcx, spcy, spcz);
         m_vd_pop_list[i]->GetResolution(resx, resy, resz);
         Vector scale;
         if (spcx>0.0 && spcy>0.0 && spcz>0.0)
         {
            scale = Vector(1.0/resx/spcx, 1.0/resy/spcy, 1.0/resz/spcz);
            scale.safe_normalize();
         }
         else
            scale = Vector(1.0, 1.0, 1.0);

         if (m_vd_pop_list[i]->GetVR())
            planes = m_vd_pop_list[i]->GetVR()->get_planes();
         if (planes && planes->size()==6)
         {
            double x1, x2, y1, y2, z1, z2;
            double abcd[4];

            (*planes)[0]->get_copy(abcd);
            x1 = fabs(abcd[3]);
            (*planes)[1]->get_copy(abcd);
            x2 = fabs(abcd[3]);
            (*planes)[2]->get_copy(abcd);
            y1 = fabs(abcd[3]);
            (*planes)[3]->get_copy(abcd);
            y2 = fabs(abcd[3]);
            (*planes)[4]->get_copy(abcd);
            z1 = fabs(abcd[3]);
            (*planes)[5]->get_copy(abcd);
            z2 = fabs(abcd[3]);

            Vector trans1(-0.5, -0.5, -0.5);
            Vector trans2(0.5, 0.5, 0.5);

            (*planes)[0]->Restore();
            (*planes)[0]->Translate(trans1);
            (*planes)[0]->Rotate(m_q_cl);
            (*planes)[0]->Scale(scale);
            (*planes)[0]->Translate(trans2);

            (*planes)[1]->Restore();
            (*planes)[1]->Translate(trans1);
            (*planes)[1]->Rotate(m_q_cl);
            (*planes)[1]->Scale(scale);
            (*planes)[1]->Translate(trans2);

            (*planes)[2]->Restore();
            (*planes)[2]->Translate(trans1);
            (*planes)[2]->Rotate(m_q_cl);
            (*planes)[2]->Scale(scale);
            (*planes)[2]->Translate(trans2);

            (*planes)[3]->Restore();
            (*planes)[3]->Translate(trans1);
            (*planes)[3]->Rotate(m_q_cl);
            (*planes)[3]->Scale(scale);
            (*planes)[3]->Translate(trans2);

            (*planes)[4]->Restore();
            (*planes)[4]->Translate(trans1);
            (*planes)[4]->Rotate(m_q_cl);
            (*planes)[4]->Scale(scale);
            (*planes)[4]->Translate(trans2);

            (*planes)[5]->Restore();
            (*planes)[5]->Translate(trans1);
            (*planes)[5]->Rotate(m_q_cl);
            (*planes)[5]->Scale(scale);
            (*planes)[5]->Translate(trans2);
         }
      }
   }
}

//sort bricks after view changes
void VRenderGLView::SetSortBricks()
{
   PopVolumeList();

   for (int i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      VolumeData* vd = m_vd_pop_list[i];
      if (vd && vd->GetTexture())
         vd->GetTexture()->set_sort_bricks();
   }
}

void VRenderGLView::SetClipMode(int mode)
{
   switch (mode)
   {
   case 0:
      m_clip_mode = 0;
      RestorePlanes();
      m_rotx_cl = 0;
      m_roty_cl = 0;
      m_rotz_cl = 0;
      break;
   case 1:
      m_clip_mode = 1;
      SetRotations(m_rotx, m_roty, m_rotz);
      break;
   case 2:
      m_clip_mode = 2;
      m_q_cl_zero.FromEuler(m_rotx, -m_roty, -m_rotz);
      m_q_cl = m_q_cl_zero;
      m_q_cl.ToEuler(m_rotx_cl, m_roty_cl, m_rotz_cl);
      if (m_rotx_cl > 180.0) m_rotx_cl -= 360.0;
      if (m_roty_cl > 180.0) m_roty_cl -= 360.0;
      if (m_rotz_cl > 180.0) m_rotz_cl -= 360.0;
      SetRotations(m_rotx, m_roty, m_rotz);
      break;
   }
}

void VRenderGLView::RestorePlanes()
{
   vector<Plane*> *planes = 0;
   for (int i=0; i<(int)m_vd_pop_list.size(); i++)
   {
      if (!m_vd_pop_list[i])
         continue;

      planes = 0;
      if (m_vd_pop_list[i]->GetVR())
         planes = m_vd_pop_list[i]->GetVR()->get_planes();
      if (planes && planes->size()==6)
      {
         (*planes)[0]->Restore();
         (*planes)[1]->Restore();
         (*planes)[2]->Restore();
         (*planes)[3]->Restore();
         (*planes)[4]->Restore();
         (*planes)[5]->Restore();
      }
   }
}

void VRenderGLView::SetClippingPlaneRotations(double rotx, double roty, double rotz)
{
   m_rotx_cl = -rotx;
   m_roty_cl = roty;
   m_rotz_cl = rotz;

   m_q_cl.FromEuler(m_rotx_cl, m_roty_cl, m_rotz_cl);
   m_q_cl.Normalize();

   SetRotations(m_rotx, m_roty, m_rotz);
}

void VRenderGLView::GetClippingPlaneRotations(double &rotx, double &roty, double &rotz)
{
   rotx = -m_rotx_cl;
   roty = m_roty_cl;
   rotz = m_rotz_cl;
}

//interpolation
void VRenderGLView::SetIntp(bool mode)
{
   m_intp = mode;
}

bool VRenderGLView::GetIntp()
{
   return m_intp;
}

//start loop update
void VRenderGLView::StartLoopUpdate()
{
   ////this is for debug_ds, comment when done
   //if (TextureRenderer::get_mem_swap() &&
   //  TextureRenderer::get_start_update_loop() &&
   //  !TextureRenderer::get_done_update_loop())
   //  return;

	int nx = GetSize().x;
	int ny = GetSize().y;

	//projection
	HandleProjection(nx, ny);
	//Transformation
	HandleCamera();

	glm::mat4 mv_temp = m_mv_mat;
	//translate object
	m_mv_mat = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	//rotate object
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	m_mv_mat = glm::rotate(m_mv_mat, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	//center object
	m_mv_mat = glm::translate(m_mv_mat, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

//	if (TextureRenderer::get_mem_swap())
	{
		PopVolumeList();
		int total_num = 0;
		int num_chan;
		//reset drawn status for all bricks
		unsigned int i, j;
		for (i=0; i<m_vd_pop_list.size(); i++)
		{
			VolumeData* vd = m_vd_pop_list[i];
			if (vd && vd->GetDisp())
			{
				switchLevel(vd);
				if (!TextureRenderer::get_mem_swap()) continue;

				vd->SetMatrices(m_mv_mat, m_proj_mat, m_tex_mat);

				num_chan = 0;
				Texture* tex = vd->GetTexture();
				if (tex)
				{
					Transform *tform = tex->transform();
					double mvmat[16];
					tform->get_trans(mvmat);
					vd->GetVR()->m_mv_mat2 = glm::mat4(
						mvmat[0], mvmat[4], mvmat[8], mvmat[12],
						mvmat[1], mvmat[5], mvmat[9], mvmat[13],
						mvmat[2], mvmat[6], mvmat[10], mvmat[14],
						mvmat[3], mvmat[7], mvmat[11], mvmat[15]);
					vd->GetVR()->m_mv_mat2 = vd->GetVR()->m_mv_mat * vd->GetVR()->m_mv_mat2;

					vector<TextureBrick*> *bricks = tex->get_bricks();
					if (bricks && bricks->size()>0)
					{
						for (j=0; j<bricks->size(); j++)
						{
							(*bricks)[j]->set_drawn(false);
							if ((*bricks)[j]->get_priority()>0 ||
								!vd->GetVR()->test_against_view((*bricks)[j]->bbox(), false))//changed by takashi
							{
								(*bricks)[j]->set_disp(false);
								continue;
							}
							else
								(*bricks)[j]->set_disp(true);

							total_num++;
							num_chan++;
							if (vd->GetMode()==1 && vd->GetShading())
							{
								total_num++;
								num_chan++;
							}
							if (vd->GetShadow())
							{
								total_num++;
								num_chan++;
							}
							if (m_draw_mask && tex->nmask() != -1)
							{
								total_num++;
								num_chan++;
							}
							if (m_draw_label && tex->nlabel() != -1)
							{
								total_num++;
								num_chan++;
							}
						}
					}
				}
				vd->SetBrickNum(num_chan);
				if (vd->GetVR())
					vd->GetVR()->set_done_loop(false);
			}
		}

		if (total_num > 0)
		{
			TextureRenderer::set_update_loop();
			TextureRenderer::set_total_brick_num(total_num);
			TextureRenderer::reset_done_current_chan();
		}
	}
}

//halt loop update
void VRenderGLView::HaltLoopUpdate()
{
   if (TextureRenderer::get_mem_swap())
   {
      TextureRenderer::reset_update_loop();
   }
}

//new function to refresh
void VRenderGLView::RefreshGL(bool erase, bool start_loop)
{
   m_draw_overlays_only = false;
   m_updating = true;
   if (start_loop)
      StartLoopUpdate();
   SetSortBricks();
   Refresh(erase);
}

//new function to refresh
void VRenderGLView::RefreshGLOverlays(bool erase)
{
	if (m_updating) return;
	if (TextureRenderer::get_mem_swap() && !TextureRenderer::get_done_update_loop()) return;

	m_draw_overlays_only = true;
	SetSortBricks();
	Refresh(erase);

}

double VRenderGLView::GetPointVolume(Point& mp, int mx, int my,
      VolumeData* vd, int mode, bool use_transf, double thresh)
{
	if (!vd)
		return -1.0;
	Texture* tex = vd->GetTexture();
	if (!tex) return -1.0;
	Nrrd* nrrd = tex->get_nrrd(0);
	if (!nrrd) return -1.0;
	void* data = nrrd->data;
	if (!data) return -1.0;

	int nx = GetSize().x;
	int ny = GetSize().y;

	if (nx <= 0 || ny <= 0)
		return -1.0;

	glm::mat4 cur_mv_mat = m_mv_mat;
	glm::mat4 cur_proj_mat = m_proj_mat;

	//projection
	HandleProjection(nx, ny);
	//Transformation
	HandleCamera();
	glm::mat4 mv_temp;
	//translate object
	mv_temp = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	//rotate object
	mv_temp = glm::rotate(mv_temp, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	mv_temp = glm::rotate(mv_temp, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	mv_temp = glm::rotate(mv_temp, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	//center object
	mv_temp = glm::translate(mv_temp, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

	Transform mv;
	Transform p;
	mv.set(glm::value_ptr(mv_temp));
	p.set(glm::value_ptr(m_proj_mat));

	double x, y;
	x = double(mx) * 2.0 / double(nx) - 1.0;
	y = 1.0 - double(my) * 2.0 / double(ny);
	p.invert();
	mv.invert();
	//transform mp1 and mp2 to object space
	Point mp1(x, y, 0.0);
	mp1 = p.transform(mp1);
	mp1 = mv.transform(mp1);
	Point mp2(x, y, 1.0);
	mp2 = p.transform(mp2);
	mp2 = mv.transform(mp2);

	//volume res
	int xx = -1;
	int yy = -1;
	int zz = -1;
	int tmp_xx, tmp_yy, tmp_zz;
	Point nmp;
	double spcx, spcy, spcz;
	vd->GetSpacings(spcx, spcy, spcz);
	int resx, resy, resz;
	vd->GetResolution(resx, resy, resz);
	//volume bounding box
	BBox bbox = vd->GetBounds();
	Vector vv = mp2 - mp1;
	vv.normalize();
	Point hit;
	double max_int = 0.0;
	double alpha = 0.0;
	double value = 0.0;
	vector<Plane*> *planes = 0;
	double mspc = 1.0;
	double return_val = -1.0;
	if (vd->GetSampleRate() > 0.0)
		mspc = sqrt(spcx*spcx + spcy*spcy + spcz*spcz)/vd->GetSampleRate();
	if (vd->GetVR())
		planes = vd->GetVR()->get_planes();
	if (bbox.intersect(mp1, vv, hit))
	{
		while (true)
		{
			tmp_xx = int(hit.x() / spcx);
			tmp_yy = int(hit.y() / spcy);
			tmp_zz = int(hit.z() / spcz);
			if (mode==1 &&
				tmp_xx==xx && tmp_yy==yy && tmp_zz==zz)
			{
				//same, skip
				hit += vv*mspc;
				continue;
			}
			else
			{
				xx = tmp_xx;
				yy = tmp_yy;
				zz = tmp_zz;
			}
			//out of bound, stop
			if (xx<0 || xx>resx ||
				yy<0 || yy>resy ||
				zz<0 || zz>resz)
				break;
			//normalize
			nmp.x(hit.x() / bbox.max().x());
			nmp.y(hit.y() / bbox.max().y());
			nmp.z(hit.z() / bbox.max().z());
			bool inside = true;
			if (planes)
				for (int i=0; i<6; i++)
					if ((*planes)[i] &&
						(*planes)[i]->eval_point(nmp)<0.0)
					{
						inside = false;
						break;
					}
					if (inside)
					{
						xx = xx==resx?resx-1:xx;
						yy = yy==resy?resy-1:yy;
						zz = zz==resz?resz-1:zz;

						if (use_transf)
							value = vd->GetTransferedValue(xx, yy, zz);
						else
							value = vd->GetOriginalValue(xx, yy, zz);

						if (mode == 1)
						{
							if (value > max_int)
							{
								mp = Point((xx+0.5)*spcx, (yy+0.5)*spcy, (zz+0.5)*spcz);
								max_int = value;
							}
						}
						else if (mode == 2)
						{
							//accumulate
							if (value > 0.0)
							{
								alpha = 1.0 - pow(Clamp(1.0-value, 0.0, 1.0), vd->GetSampleRate());
								max_int += alpha*(1.0-max_int);
								mp = Point((xx+0.5)*spcx, (yy+0.5)*spcy, (zz+0.5)*spcz);
							}
							if (max_int >= thresh)
								break;
						}
					}
					hit += vv*mspc;
		}
	}
	
	if (mode==1)
	{
		if (max_int > 0.0)
			return_val = (mp-mp1).length();
	}
	else if (mode==2)
	{
		if (max_int >= thresh)
			return_val = (mp-mp1).length();
	}
	
	m_mv_mat = cur_mv_mat;
	m_proj_mat = cur_proj_mat;
	return return_val;
}

double VRenderGLView::GetPointVolumeBox(Point &mp, int mx, int my, VolumeData* vd, bool calc_mats)
{
   if (!vd)
		return -1.0;
	int nx = GetSize().x;
	int ny = GetSize().y;
	if (nx <= 0 || ny <= 0)
		return -1.0;
	vector<Plane*> *planes = vd->GetVR()->get_planes();
	if (planes->size() != 6)
		return -1.0;

	Transform mv;
	Transform p;
	glm::mat4 mv_temp = m_mv_mat;
	Transform *tform = vd->GetTexture()->transform();
	double mvmat[16];
	tform->get_trans(mvmat);

	glm::mat4 cur_mv_mat;
	glm::mat4 cur_proj_mat;

	if (calc_mats)
	{
		cur_mv_mat = m_mv_mat;
		cur_proj_mat = m_proj_mat;

		//projection
		HandleProjection(nx, ny);
		//Transformation
		HandleCamera();
		//translate object
		mv_temp = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
		//rotate object
		mv_temp = glm::rotate(mv_temp, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
		mv_temp = glm::rotate(mv_temp, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
		mv_temp = glm::rotate(mv_temp, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
		//center object
		mv_temp = glm::translate(mv_temp, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));
	}
	else
		mv_temp = m_mv_mat;

	glm::mat4 mv_mat2 = glm::mat4(
		mvmat[0], mvmat[4], mvmat[8], mvmat[12],
		mvmat[1], mvmat[5], mvmat[9], mvmat[13],
		mvmat[2], mvmat[6], mvmat[10], mvmat[14],
		mvmat[3], mvmat[7], mvmat[11], mvmat[15]);
	mv_temp = mv_temp * mv_mat2;

	mv.set(glm::value_ptr(mv_temp));
	p.set(glm::value_ptr(m_proj_mat));

	double x, y;
	x = double(mx) * 2.0 / double(nx) - 1.0;
	y = 1.0 - double(my) * 2.0 / double(ny);
	p.invert();
	mv.invert();
	//transform mp1 and mp2 to object space
	Point mp1(x, y, 0.0);
	mp1 = p.transform(mp1);
	mp1 = mv.transform(mp1);
	Point mp2(x, y, 1.0);
	mp2 = p.transform(mp2);
	mp2 = mv.transform(mp2);
	Vector ray_d = mp1-mp2;
	ray_d.normalize();
	Ray ray(mp1, ray_d);
	double mint = -1.0;
	double t;
	//for each plane, calculate the intersection point
	Plane* plane = 0;
	Point pp;//a point on plane
	int i, j;
	bool pp_out;
	for (i=0; i<6; i++)
	{
		plane = (*planes)[i];
		FLIVR::Vector vec = plane->normal();
		FLIVR::Point pnt = plane->get_point();
		if (ray.planeIntersectParameter(vec,pnt, t))
		{
			pp = ray.parameter(t);

			pp_out = false;
			//determine if the point is inside the box
			for (j=0; j<6; j++)
			{
				if (j == i)
					continue;
				if ((*planes)[j]->eval_point(pp) < 0)
				{
					pp_out = true;
					break;
				}
			}

			if (!pp_out)
			{
				if (t > mint)
				{
					mp = pp;
					mint = t;
				}
			}
		}
	}

	mp = tform->transform(mp);

	if (calc_mats)
	{
		m_mv_mat = cur_mv_mat;
		m_proj_mat = cur_proj_mat;
	}

	return mint;
}

double VRenderGLView::GetPointPlane(Point &mp, int mx, int my, Point* planep, bool calc_mats)
{
	int nx = GetSize().x;
	int ny = GetSize().y;

	if (nx <= 0 || ny <= 0)
		return -1.0;

	glm::mat4 mv_temp;
	glm::mat4 cur_mv_mat;
	glm::mat4 cur_proj_mat;

	if (calc_mats)
	{
		cur_mv_mat = m_mv_mat;
		cur_proj_mat = m_proj_mat;

		//projection
		HandleProjection(nx, ny);
		//Transformation
		HandleCamera();
		//translate object
		mv_temp = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
		//rotate object
		mv_temp = glm::rotate(mv_temp, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
		mv_temp = glm::rotate(mv_temp, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
		mv_temp = glm::rotate(mv_temp, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
		//center object
		mv_temp = glm::translate(mv_temp, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));
	}
	else
		mv_temp = m_mv_mat;

	Transform mv;
	Transform p;
	mv.set(glm::value_ptr(mv_temp));
	p.set(glm::value_ptr(m_proj_mat));

	Vector n(0.0, 0.0, 1.0);
	Point center(0.0, 0.0, -m_distance);
	if (planep)
	{
		center = *planep;
		center = mv.transform(center);
	}
	double x, y;
	x = double(mx) * 2.0 / double(nx) - 1.0;
	y = 1.0 - double(my) * 2.0 / double(ny);
	p.invert();
	mv.invert();
	//transform mp1 and mp2 to eye space
	Point mp1(x, y, 0.0);
	mp1 = p.transform(mp1);
	Point mp2(x, y, 1.0);
	mp2 = p.transform(mp2);
	FLIVR::Vector vec = mp2-mp1;
	Ray ray(mp1, vec);
	double t = 0.0;
	if (ray.planeIntersectParameter(n, center, t))
		mp = ray.parameter(t);
	//transform mp to world space
	mp = mv.transform(mp);

	if (calc_mats)
	{
		m_mv_mat = cur_mv_mat;
		m_proj_mat = cur_proj_mat;
	}

	return (mp-mp1).length();
}

Point* VRenderGLView::GetEditingRulerPoint(int mx, int my)
{
	Point* point = 0;

	int nx = GetSize().x;
	int ny = GetSize().y;

	if (nx <= 0 || ny <= 0)
		return 0;

	double x, y;
	x = double(mx) * 2.0 / double(nx) - 1.0;
	y = 1.0 - double(my) * 2.0 / double(ny);
	double aspect = (double)nx / (double)ny;

	glm::mat4 cur_mv_mat = m_mv_mat;
	glm::mat4 cur_proj_mat = m_proj_mat;

	//projection
	HandleProjection(nx, ny);
	//Transformation
	HandleCamera();
	glm::mat4 mv_temp;
	//translate object
	mv_temp = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	//rotate object
	mv_temp = glm::rotate(mv_temp, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	mv_temp = glm::rotate(mv_temp, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	mv_temp = glm::rotate(mv_temp, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	//center object
	mv_temp = glm::translate(mv_temp, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

	Transform mv;
	Transform p;
	mv.set(glm::value_ptr(mv_temp));
	p.set(glm::value_ptr(m_proj_mat));

   int i, j;
   Point ptemp;
   bool found = false;
   for (i=0; i<(int)m_ruler_list.size() && !found; i++)
   {
      Ruler* ruler = m_ruler_list[i];
      if (!ruler) continue;

      for (j=0; j<ruler->GetNumPoint(); j++)
      {
         point = ruler->GetPoint(j);
         if (!point) continue;
         ptemp = *point;
         ptemp = mv.transform(ptemp);
         ptemp = p.transform(ptemp);
         if ((m_persp && (ptemp.z()<=0.0 || ptemp.z()>=1.0)) ||
               (!m_persp && (ptemp.z()>=0.0 || ptemp.z()<=-1.0)))
            continue;
         if (x<ptemp.x()+0.02/aspect &&
               x>ptemp.x()-0.02/aspect &&
               y<ptemp.y()+0.02 &&
               y>ptemp.y()-0.02)
         {
            found = true;
            break;
         }
      }
   }

   if(!found && m_draw_landmarks)
   {
	   for (i=0; i<(int)m_landmarks.size() && !found; i++)
	   {
		   Ruler* ruler = m_landmarks[i];
		   if (!ruler) continue;

		   for (j=0; j<ruler->GetNumPoint(); j++)
		   {
			   point = ruler->GetPoint(j);
			   if (!point) continue;
			   ptemp = *point;
			   ptemp = mv.transform(ptemp);
			   ptemp = p.transform(ptemp);
			   if ((m_persp && (ptemp.z()<=0.0 || ptemp.z()>=1.0)) ||
				   (!m_persp && (ptemp.z()>=0.0 || ptemp.z()<=-1.0)))
				   continue;
			   if (x<ptemp.x()+0.02/aspect &&
				   x>ptemp.x()-0.02/aspect &&
				   y<ptemp.y()+0.02 &&
				   y>ptemp.y()-0.02)
			   {
				   found = true;
				   break;
			   }
		   }
	   }
   }

   m_mv_mat = cur_mv_mat;
   m_proj_mat = cur_proj_mat;

   if (found)
      return point;
   else
      return 0;
}

//added by takashi
bool VRenderGLView::SwitchRulerBalloonVisibility_Point(int mx, int my)
{
	Point* point = 0;

	int nx = GetSize().x;
	int ny = GetSize().y;

	if (nx <= 0 || ny <= 0)
		return 0;

	double x, y;
	x = double(mx) * 2.0 / double(nx) - 1.0;
	y = 1.0 - double(my) * 2.0 / double(ny);
	double aspect = (double)nx / (double)ny;

	glm::mat4 cur_mv_mat = m_mv_mat;
	glm::mat4 cur_proj_mat = m_proj_mat;

	//projection
	HandleProjection(nx, ny);
	//Transformation
	HandleCamera();
	glm::mat4 mv_temp;
	//translate object
	mv_temp = glm::translate(m_mv_mat, glm::vec3(m_obj_transx, m_obj_transy, m_obj_transz));
	//rotate object
	mv_temp = glm::rotate(mv_temp, float(m_obj_roty+180.0), glm::vec3(0.0, 1.0, 0.0));
	mv_temp = glm::rotate(mv_temp, float(m_obj_rotz+180.0), glm::vec3(0.0, 0.0, 1.0));
	mv_temp = glm::rotate(mv_temp, float(m_obj_rotx), glm::vec3(1.0, 0.0, 0.0));
	//center object
	mv_temp = glm::translate(mv_temp, glm::vec3(-m_obj_ctrx, -m_obj_ctry, -m_obj_ctrz));

	Transform mv;
	Transform p;
	mv.set(glm::value_ptr(mv_temp));
	p.set(glm::value_ptr(m_proj_mat));

	int i, j;
   Point ptemp;
   bool found = false;
   for (i=0; i<(int)m_ruler_list.size() && !found; i++)
   {
      Ruler* ruler = m_ruler_list[i];
      if (!ruler) continue;

      for (j=0; j<ruler->GetNumPoint(); j++)
      {
         point = ruler->GetPoint(j);
         if (!point) continue;
         ptemp = *point;
         ptemp = mv.transform(ptemp);
         ptemp = p.transform(ptemp);
         if ((m_persp && (ptemp.z()<=0.0 || ptemp.z()>=1.0)) ||
               (!m_persp && (ptemp.z()>=0.0 || ptemp.z()<=-1.0)))
            continue;
         if (x<ptemp.x()+0.02/aspect &&
               x>ptemp.x()-0.02/aspect &&
               y<ptemp.y()+0.02 &&
               y>ptemp.y()-0.02)
         {
			ruler->SetBalloonVisibility(j, !ruler->GetBalloonVisibility(j));
            found = true;
            break;
         }
      }
   }

   if(!found)
   {
	   for (i=0; i<(int)m_landmarks.size() && !found; i++)
	   {
		   Ruler* ruler = m_landmarks[i];
		   if (!ruler) continue;

		   for (j=0; j<ruler->GetNumPoint(); j++)
		   {
			   point = ruler->GetPoint(j);
			   if (!point) continue;
			   ptemp = *point;
			   ptemp = mv.transform(ptemp);
			   ptemp = p.transform(ptemp);
			   if ((m_persp && (ptemp.z()<=0.0 || ptemp.z()>=1.0)) ||
				   (!m_persp && (ptemp.z()>=0.0 || ptemp.z()<=-1.0)))
				   continue;
			   if (x<ptemp.x()+0.02/aspect &&
				   x>ptemp.x()-0.02/aspect &&
				   y<ptemp.y()+0.02 &&
				   y>ptemp.y()-0.02)
			   {
				   ruler->SetBalloonVisibility(j, !ruler->GetBalloonVisibility(j));
				   found = true;
				   break;
			   }
		   }
	   }
   }

   m_mv_mat = cur_mv_mat;
   m_proj_mat = cur_proj_mat;

   if (found)
      return true;
   else
      return false;
}

int VRenderGLView::GetRulerType()
{
   return m_ruler_type;
}

void VRenderGLView::SetRulerType(int type)
{
   m_ruler_type = type;
}

void VRenderGLView::FinishRuler()
{
   size_t size = m_ruler_list.size();
   if (!size) return;
   if (m_ruler_list[size-1] &&
         m_ruler_list[size-1]->GetRulerType() == 1)
      m_ruler_list[size-1]->SetFinished();
}

bool VRenderGLView::GetRulerFinished()
{
   size_t size = m_ruler_list.size();
   if (!size) return true;
   if (m_ruler_list[size-1])
      return m_ruler_list[size-1]->GetFinished();
   else
      return true;
}

void VRenderGLView::AddRulerPoint(int mx, int my)
{
   Point p;

   if (m_point_volume_mode)
   {
      double t = GetPointVolume(p, mx, my, m_cur_vol,
            m_point_volume_mode, m_ruler_use_transf);
      if (t <= 0.0)
      {
         t = GetPointPlane(p, mx, my);
         if (t <= 0.0)
            return;
      }
   }
   else
   {
      double t = GetPointPlane(p, mx, my);
      if (t <= 0.0)
         return;
   }

   bool new_ruler = true;
   if (m_ruler_list.size())
   {
      Ruler* ruler = m_ruler_list[m_ruler_list.size()-1];
      if (ruler && !ruler->GetFinished())
      {
         ruler->AddPoint(p);
         new_ruler = false;
      }
   }
   if (new_ruler)
   {
      Ruler* ruler = new Ruler();
      ruler->SetRulerType(m_ruler_type);
      ruler->AddPoint(p);
      ruler->SetTimeDep(m_ruler_time_dep);
      ruler->SetTime(m_tseq_cur_num);
      m_ruler_list.push_back(ruler);
   }

   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (m_vrv && vr_frame && vr_frame->GetMeasureDlg())
      vr_frame->GetMeasureDlg()->GetSettings(m_vrv);
}

void VRenderGLView::AddPaintRulerPoint()
{
   if (m_selector.ProcessSel(0.01))
   {
      wxString str;
      Point center;
      double size;
      m_selector.GetCenter(center);
      m_selector.GetSize(size);

      bool new_ruler = true;
      if (m_ruler_list.size())
      {
         Ruler* ruler = m_ruler_list[m_ruler_list.size()-1];
         if (ruler && !ruler->GetFinished())
         {
            ruler->AddPoint(center);
            str = wxString::Format("\tv%d", ruler->GetNumPoint()-1);
            ruler->AddInfoNames(str);
            str = wxString::Format("\t%.0f", size);
            ruler->AddInfoValues(str);
            new_ruler = false;
         }
      }
      if (new_ruler)
      {
         Ruler* ruler = new Ruler();
         ruler->SetRulerType(m_ruler_type);
         ruler->AddPoint(center);
         ruler->SetTimeDep(m_ruler_time_dep);
         ruler->SetTime(m_tseq_cur_num);
         str = "v0";
         ruler->AddInfoNames(str);
         str = wxString::Format("%.0f", size);
         ruler->AddInfoValues(str);
         m_ruler_list.push_back(ruler);
      }

      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (m_vrv && vr_frame && vr_frame->GetMeasureDlg())
         vr_frame->GetMeasureDlg()->GetSettings(m_vrv);
   }
}

void VRenderGLView::DrawRulers()
{
   int nx = GetSize().x;
   int ny = GetSize().y;

   if (nx <= 0 || ny <= 0)
      return;

   double spcx = 1.0;
   double spcy = 1.0;
   double spcz = 1.0;

   if(m_cur_vol){
	   Texture *vtex = m_cur_vol->GetTexture();
	   if (vtex && vtex->isBrxml())
	   {
		   BRKXMLReader *br = (BRKXMLReader *)m_cur_vol->GetReader();
		   br->SetLevel(0);
		   spcx = br->GetXSpc();
		   spcy = br->GetYSpc();
		   spcz = br->GetZSpc();
	   }
   }

   for (int i=0; i<(int)m_ruler_list.size(); i++)
   {
      Ruler* ruler = m_ruler_list[i];
      if (!ruler) continue;
      if (!ruler->GetTimeDep() ||
            (ruler->GetTimeDep() &&
             ruler->GetTime() == m_tseq_cur_num))
			 ruler->Draw(m_persp, nx, ny, m_mv_mat, m_proj_mat, m_cur_vol ? m_cur_vol->GetAnnotation() : vector<AnnotationDB>(), spcx, spcy, spcz);
   }

   vector<VolumeData *> displist;
   for (int i=(int)m_layer_list.size()-1; i>=0; i--)
   {
	   if (!m_layer_list[i])
		   continue;
	   switch (m_layer_list[i]->IsA())
	   {
	   case 2://volume data (this won't happen now)
		   {
			   VolumeData* vd = (VolumeData*)m_layer_list[i];
			   if (vd && vd->GetDisp())
			   {
				   displist.push_back(vd);
			   }
		   }
		   break;
	   case 5://group
		   {
			   DataGroup* group = (DataGroup*)m_layer_list[i];
			   if (!group->GetDisp())
				   continue;
			   for (int j=group->GetVolumeNum()-1; j>=0; j--)
			   {
				   VolumeData* vd = group->GetVolumeData(j);
				   if (vd && vd->GetDisp())
				   {
					   displist.push_back(vd);
				   }
			   }
		   }
		   break;
	   }
   }
   sort(displist.begin(), displist.end());
   sort(m_lm_vdlist.begin(), m_lm_vdlist.end());

   if (m_lm_vdlist != displist)
   {
	   for(int i = 0; i < m_landmarks.size(); i++)
	   {
		   delete m_landmarks[i];
		   m_landmarks[i] = NULL;
	   }
	   vector<Ruler *>().swap(m_landmarks);

	   m_lm_vdlist = displist;

	   if(m_lm_vdlist.size() > 0)
	   {
		   int id1;
		   wstring lmid;
		   for(id1 = 1; id1 < m_lm_vdlist.size(); id1++)
		   {
			   if (m_lm_vdlist[id1]->GetLandmarkNum() > 0)
			   {
				   m_lm_vdlist[id1]->GetMetadataID(lmid);
				   break;
			   }
		   }

		   bool multi = false;
		   for(int i = id1; i < m_lm_vdlist.size(); i++)
		   {
			   wstring lmid2;
			   m_lm_vdlist[i]->GetMetadataID(lmid2);
			   if (lmid != lmid2 && m_lm_vdlist[i]->GetLandmarkNum() > 0)
			   {
				   multi = true;
				   break;
			   }
		   }

		   map<wstring, VD_Landmark> lmmap;
		   vector<pair<wstring, wstring>> redundant_first;
		   for(int i = 0; i < m_lm_vdlist.size(); i++)
		   {
			   int lmnum = m_lm_vdlist[i]->GetLandmarkNum();
			   for(int j = 0; j < lmnum; j++)
			   {
				   VD_Landmark lm;
				   m_lm_vdlist[i]->GetLandmark(j, lm);
				   if (multi) 
				   {
					   wstring name = lm.name;
					   m_lm_vdlist[i]->GetMetadataID(lmid);
					   if(!lmid.empty())
					   {
						   name += L" (";
						   name += lmid;
						   name += L")";
						   lm.name = name;
					   }
				   }

				   if (lmmap.find(lm.name) == lmmap.end())
				   {
					   lmmap[lm.name] = lm;
					   Ruler* ruler = new Ruler();
					   ruler->SetNameDisp(lm.name);
					   ruler->SetRulerType(2);
					   ruler->AddPoint(Point(lm.x, lm.y, lm.z));
					   ruler->SetTimeDep(false);
					   m_landmarks.push_back(ruler);
				   }
			   }
		   }
	   }
   }

   if(m_draw_landmarks)
   {
	   for (int i=0; i<(int)m_landmarks.size(); i++)
	   {
		   Ruler* ruler = m_landmarks[i];
		   if (!ruler) continue;
		   if (!ruler->GetTimeDep() ||
			   (ruler->GetTimeDep() &&
			   ruler->GetTime() == m_tseq_cur_num))
			   ruler->Draw(m_persp, nx, ny, m_mv_mat, m_proj_mat, m_cur_vol ? m_cur_vol->GetAnnotation() : vector<AnnotationDB>(), spcx, spcy, spcz);
	   }
   }
}

vector<Ruler*>* VRenderGLView::GetRulerList()
{
   return &m_ruler_list;
}

//traces
TraceGroup* VRenderGLView::GetTraceGroup()
{
   return m_trace_group;
}

void VRenderGLView::CreateTraceGroup()
{
	if (!m_trace_group)
		m_trace_group = new TraceGroup;
}

int VRenderGLView::LoadTraceGroup(wxString filename)
{
   if (m_trace_group)
      delete m_trace_group;

   m_trace_group = new TraceGroup;
   return m_trace_group->Load(filename);
}

void VRenderGLView::ExportTrace(wxString filename, unsigned int id)
{
   if (!m_trace_group)
      return;
}

void VRenderGLView::DrawTraces()
{
   if (!m_trace_group)
      return;
   m_trace_group->Draw(m_mv_mat, m_proj_mat);
}

void VRenderGLView::GetTraces()
{
	if (!m_trace_group)
		return;

	int ii, jj, kk;
	int nx, ny, nz;
	//return current mask (into system memory)
	if (!m_cur_vol) return;
	m_cur_vol->GetVR()->return_mask();
	m_cur_vol->GetResolution(nx, ny, nz);
	//find labels in the old that are selected by the current mask
	Texture* tex = m_cur_vol->GetTexture();
	if (!tex) return;
	Nrrd* mask_nrrd = tex->get_nrrd(tex->nmask());
	if (!mask_nrrd) return;
	Nrrd* label_nrrd = tex->get_nrrd(tex->nlabel());
	if(!label_nrrd) return;
	unsigned char* mask_data = (unsigned char*)(mask_nrrd->data);
	if (!mask_data) return;
	unsigned int* label_data = (unsigned int*)(label_nrrd->data);
	if (!label_data) return;
	unordered_map<unsigned int, Lbl> sel_labels;
	unordered_map<unsigned int, Lbl>::iterator label_iter;
	for (ii=0; ii<nx; ii++)
	for (jj=0; jj<ny; jj++)
	for (kk=0; kk<nz; kk++)
	{
		int index = nx*ny*kk + nx*jj + ii;
		unsigned int label_value = label_data[index];
		if (mask_data[index] && label_value)
		{
			label_iter = sel_labels.find(label_value);
			if (label_iter == sel_labels.end())
			{
				Lbl lbl;
				lbl.id = label_value;
				lbl.size = 1;
				sel_labels.insert(pair<unsigned int, Lbl>(label_value, lbl));
			}
			else
				label_iter->second.size++;
		}
	}

	//create id list
	m_trace_group->SetCurTime(m_tseq_cur_num);
	m_trace_group->SetPrvTime(m_tseq_cur_num);
	m_trace_group->SetIDMap(sel_labels);

	//add traces to trace dialog
	VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
	if (m_vrv && vr_frame && vr_frame->GetTraceDlg())
		vr_frame->GetTraceDlg()->GetSettings(m_vrv);
}

/*WXLRESULT VRenderGLView::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
  {
  PACKET pkt;

  if (message == WT_PACKET)
  {
  if (gpWTPacket((HCTX)lParam, wParam, &pkt))
  {
  unsigned int prsNew = pkt.pkNormalPressure;
  wxSize size = GetClientSize();
  long pkx = (size.GetX() * pkt.pkX) / m_lc.lcOutExtX;
  long pky = size.GetY() - (size.GetY() * pkt.pkY) / m_lc.lcOutExtY;

  m_paint_enable = false;

  if (HIWORD(pkt.pkButtons)==TBN_DOWN)
  {
  if (m_int_mode == 2)
  {
  old_mouse_X = pkx;
  old_mouse_Y = pky;
  prv_mouse_X = old_mouse_X;
  prv_mouse_Y = old_mouse_Y;
  m_paint_enable = true;
  m_clear_paint = true;
  m_on_press = true;
  RefreshGL();
  }
  }

  if (HIWORD(pkt.pkButtons)==TBN_UP)
  {
  if (m_int_mode == 2)
  {
  m_paint_enable = true;
  Segment();
  m_int_mode = 1;
  m_on_press = false;
  RefreshGL();
  }
  }

//update mouse position
if (old_mouse_X >= 0 && old_mouse_Y >=0)
{
prv_mouse_X = old_mouse_X;
prv_mouse_Y = old_mouse_Y;
old_mouse_X = pkx;
old_mouse_Y = pky;
}
else
{
old_mouse_X = pkx;
old_mouse_Y = pky;
prv_mouse_X = old_mouse_X;
prv_mouse_Y = old_mouse_Y;
}

if (m_int_mode == 2 && m_on_press)
{
m_paint_enable = true;
RefreshGL();
}

if (m_on_press)
m_pressure = double(prsNew)/512.0;
else
m_pressure = 1.0;

if (m_draw_brush)
RefreshGL();
}
}

return wxWindow::MSWWindowProc(message, wParam, lParam);
}*/

void VRenderGLView::OnMouse(wxMouseEvent& event)
{
   //mouse interactive flag
   //m_interactive = false; //deleted by takashi
   m_paint_enable = false;
   m_drawing_coord = false;

   wxPoint mouse_pos = wxGetMousePosition();
   wxRect view_reg = GetScreenRect();
   if (view_reg.Contains(mouse_pos))
      UpdateBrushState();

   extern CURLM * _g_curlm;
   int handle_count;
   curl_multi_perform(_g_curlm, &handle_count);

   //added by takashi
   if(event.LeftDClick())
   {
	   Point *p = GetEditingRulerPoint(event.GetX(), event.GetY());
	   if(p)
	   {
		   double cx, cy, cz;
		   GetObjCenters(cx, cy, cz);
		   StartManipulation(NULL, NULL, NULL, &Point(cx-p->x(), -(cy-p->y()), -(cz-p->z())), NULL);
	   }
   }
   
   //mouse button down operations
   if (event.LeftDown())
   {
	   m_searcher->KillFocus();

      if (m_int_mode == 6)
	  {
         m_editing_ruler_point = GetEditingRulerPoint(event.GetX(), event.GetY());
		 old_mouse_X = event.GetX();
         old_mouse_Y = event.GetY();
	  }

      if (m_int_mode == 1 ||
            (m_int_mode==5 &&
             event.AltDown()) ||
            (m_int_mode==6 &&
             m_editing_ruler_point==0))
      {
         old_mouse_X = event.GetX();
         old_mouse_Y = event.GetY();
         m_pick = true;
      }
      else if (m_int_mode == 2 || m_int_mode == 7)
      {
         old_mouse_X = event.GetX();
         old_mouse_Y = event.GetY();
         prv_mouse_X = old_mouse_X;
         prv_mouse_Y = old_mouse_Y;
         m_paint_enable = true;
         //m_clear_paint = true;
         RefreshGLOverlays();
      }
      return;
   }
   if (event.RightDown())
   {
	   m_searcher->KillFocus();

      old_mouse_X = event.GetX();
      old_mouse_Y = event.GetY();
      return;
   }
   if (event.MiddleDown())
   {
	   m_searcher->KillFocus();

      old_mouse_X = event.GetX();
      old_mouse_Y = event.GetY();
      return;
   }

   //mouse button up operations
   if (event.LeftUp())
   {
      if (m_int_mode == 1)
      {
         //pick polygon models
         if (m_pick)
            Pick();
      }
      else if (m_int_mode == 2)
      {
         //segment volumes
         m_paint_enable = true;
         Segment();
         m_int_mode = 4;
         m_force_clear = true;
		 m_clear_paint = true;
      }
      else if (m_int_mode == 5 &&
            !event.AltDown())
      {
         //add one point to a ruler
         AddRulerPoint(event.GetX(), event.GetY());
		 RefreshGLOverlays();
		 return;
      }
      else if (m_int_mode == 6)
      {
         m_editing_ruler_point = 0;
      }
      else if (m_int_mode == 7)
      {
         //segment volume, calculate center, add ruler point
         m_paint_enable = true;
         Segment();
         AddPaintRulerPoint();
         m_int_mode = 8;
         m_force_clear = true;
      }
      //SetSortBricks();
      RefreshGL();
      return;
   }
   if (event.MiddleUp())
   {
      //SetSortBricks();
      RefreshGL();
      return;
   }
   if (event.RightUp())
   {
      if (m_int_mode == 5 &&
            !event.AltDown())
      {
         if (GetRulerFinished())
         {
            SetIntMode(1);
            VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
            if (m_vrv && vr_frame && vr_frame->GetMeasureDlg())
               vr_frame->GetMeasureDlg()->GetSettings(m_vrv);
         }
         else
         {
            AddRulerPoint(event.GetX(), event.GetY());
            FinishRuler();
         }
      }
	  
	  //added by takashi
	  SwitchRulerBalloonVisibility_Point(event.GetX(), event.GetY());

      //SetSortBricks();
      RefreshGL();
      return;
   }

   //mouse dragging
   if (event.Dragging())
   {
      TextureRenderer::set_cor_up_time(
            int(sqrt(double(old_mouse_X-event.GetX())*
                  double(old_mouse_X-event.GetX())+
                  double(old_mouse_Y-event.GetY())*
                  double(old_mouse_Y-event.GetY()))));

      bool hold_old = false;
      if (m_int_mode == 1 ||
            (m_int_mode==5 &&
             event.AltDown()) ||
            (m_int_mode==6 &&
             m_editing_ruler_point==0))
      {
         //disable picking
         m_pick = false;

         if (old_mouse_X!=-1&&
               old_mouse_Y!=-1&&
               abs(old_mouse_X-event.GetX())+
               abs(old_mouse_Y-event.GetY())<200)
         {
            if (event.LeftIsDown() && !event.ControlDown())
            {
               Quaternion q_delta = Trackball(old_mouse_X, event.GetY(), event.GetX(), old_mouse_Y);
               if (m_rot_lock && q_delta.IsIdentity())
                  hold_old = true;
               m_q *= q_delta;
               m_q.Normalize();
               Quaternion cam_pos(0.0, 0.0, m_distance, 0.0);
               Quaternion cam_pos2 = (-m_q) * cam_pos * m_q;
               m_transx = cam_pos2.x;
               m_transy = cam_pos2.y;
               m_transz = cam_pos2.z;

               Quaternion up(0.0, 1.0, 0.0, 0.0);
               Quaternion up2 = (-m_q) * up * m_q;
               m_up = Vector(up2.x, up2.y, up2.z);

               Q2A();

               wxString str = wxString::Format("%.1f", m_rotx);
               m_vrv->m_x_rot_text->ChangeValue(str);
               if (m_rot_lock)
                  m_vrv->m_x_rot_sldr->SetValue(int(m_rotx/45.0));
               else
                  m_vrv->m_x_rot_sldr->SetValue(int(m_rotx));
               str = wxString::Format("%.1f", m_roty);
               m_vrv->m_y_rot_text->ChangeValue(str);
               if (m_rot_lock)
                  m_vrv->m_y_rot_sldr->SetValue(int(m_roty/45.0));
               else
                  m_vrv->m_y_rot_sldr->SetValue(int(m_roty));
               str = wxString::Format("%.1f", m_rotz);
               m_vrv->m_z_rot_text->ChangeValue(str);
               if (m_rot_lock)
                  m_vrv->m_z_rot_sldr->SetValue(int(m_rotz/45.0));
               else
                  m_vrv->m_z_rot_sldr->SetValue(int(m_rotz));

               m_interactive = m_adaptive;
			   m_int_res = m_adaptive_res;

               if (m_linked_rot)
               {
                  VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
                  if (vr_frame)
                  {
                     for (int i=0; i<vr_frame->GetViewNum(); i++)
                     {
                        VRenderView* view = vr_frame->GetView(i);
                        if (view && view->m_glview &&
                              view->m_glview!=this &&
                              view->m_glview->m_linked_rot)
                        {
                           view->m_glview->SetRotations(m_rotx, m_roty, m_rotz, true, false);
                           view->RefreshGL();
                        }
                     }
                  }
               }
               if (!hold_old)
                  RefreshGL();
            }
            if (event.MiddleIsDown() || (event.ControlDown() && event.LeftIsDown()))
            {
               long dx = event.GetX() - old_mouse_X;
               long dy = event.GetY() - old_mouse_Y;

               m_head = Vector(-m_transx, -m_transy, -m_transz);
               m_head.normalize();
               Vector side = Cross(m_up, m_head);
               Vector trans = -(
                     side*(double(dx)*(m_ortho_right-m_ortho_left)/double(GetSize().x))+
                     m_up*(double(dy)*(m_ortho_top-m_ortho_bottom)/double(GetSize().y)));
               m_obj_transx += trans.x();
               m_obj_transy += trans.y();
               m_obj_transz += trans.z();

               m_interactive = m_adaptive;
			   m_int_res = m_adaptive_res;

               //SetSortBricks();
               RefreshGL();
            }
            if (event.RightIsDown())
            {
               long dx = event.GetX() - old_mouse_X;
               long dy = event.GetY() - old_mouse_Y;

               double delta = abs(dx)>abs(dy)?
                  (double)dx/(double)GetSize().x:
                  (double)-dy/(double)GetSize().y;
               m_scale_factor += m_scale_factor*delta;
               wxString str = wxString::Format("%.0f", m_scale_factor*100.0);
               m_vrv->m_scale_factor_sldr->SetValue(m_scale_factor*100);
               m_vrv->m_scale_factor_text->ChangeValue(str);

               if (m_free)
               {
                  Vector pos(m_transx, m_transy, m_transz);
                  pos.normalize();
                  Vector ctr(m_ctrx, m_ctry, m_ctrz);
                  ctr -= delta*pos*1000;
                  m_ctrx = ctr.x();
                  m_ctry = ctr.y();
                  m_ctrz = ctr.z();
               }

               m_interactive = m_adaptive;
			   m_int_res = m_adaptive_res;

               //SetSortBricks();
               RefreshGL();
            }
         }
      }
      else if (m_int_mode == 2 || m_int_mode == 7)
      {
         m_paint_enable = true;
         m_pressure = 1.0;
         RefreshGLOverlays();
      }
      else if (m_int_mode ==3)
      {
         if (old_mouse_X!=-1&&
               old_mouse_Y!=-1&&
               abs(old_mouse_X-event.GetX())+
               abs(old_mouse_Y-event.GetY())<200)
         {
            if (event.LeftIsDown())
            {
               Quaternion q_delta = TrackballClip(old_mouse_X, event.GetY(), event.GetX(), old_mouse_Y);
               m_q_cl = q_delta * m_q_cl;
               m_q_cl.Normalize();
               SetRotations(m_rotx, m_roty, m_rotz);
               RefreshGL();
            }
         }
      }
      else if (m_int_mode == 6)
      {
		 if (event.LeftIsDown() &&
               m_editing_ruler_point)
         {
            Point point;
            bool failed = false;
            if (m_point_volume_mode)
            {
               double t = GetPointVolume(point,
                     event.GetX(), event.GetY(),
                     m_cur_vol, m_point_volume_mode,
                     m_ruler_use_transf);
               if (t <= 0.0)
                  t = GetPointPlane(point,
                        event.GetX(), event.GetY(),
                        m_editing_ruler_point);
               if (t <= 0.0)
                  failed = true;
            }
            else
            {
               double t = GetPointPlane(point,
                     event.GetX(), event.GetY(),
                     m_editing_ruler_point);
               if (t <= 0.0)
                  failed = true;
            }

            if (!failed)
            {
               m_editing_ruler_point->x(point.x());
               m_editing_ruler_point->y(point.y());
               m_editing_ruler_point->z(point.z());

			   //added by takashi
			   //m_interactive = m_adaptive;

			   RefreshGLOverlays();
			   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
			   if (m_vrv && vr_frame && vr_frame->GetMeasureDlg())
				   vr_frame->GetMeasureDlg()->GetSettings(m_vrv);
            }
         }
	  }

      //update mouse position
      if (old_mouse_X >= 0 && old_mouse_Y >=0)
      {
         prv_mouse_X = old_mouse_X;
         prv_mouse_Y = old_mouse_Y;
         if (!hold_old)
         {
            old_mouse_X = event.GetX();
            old_mouse_Y = event.GetY();
         }
      }
      else
      {
         old_mouse_X = event.GetX();
         old_mouse_Y = event.GetY();
         prv_mouse_X = old_mouse_X;
         prv_mouse_Y = old_mouse_Y;
      }
      return;
   }

   //wheel operations
   int wheel = event.GetWheelRotation();
   if (wheel)  //if rotation
   {
      if (m_int_mode == 2 || m_int_mode == 7)
      {
         if (m_use_brush_radius2)
         {
            double delta = wheel / 100.0;
            m_brush_radius2 += delta;
            m_brush_radius2 = max(1.0, m_brush_radius2);
            m_brush_radius1 = min(m_brush_radius2, m_brush_radius1);
         }
         else
         {
            m_brush_radius1 += wheel / 100.0;
            m_brush_radius1 = max(m_brush_radius1, 1.0);
            m_brush_radius2 = m_brush_radius1;
         }

         VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
         if (vr_frame && vr_frame->GetBrushToolDlg())
            vr_frame->GetBrushToolDlg()->GetSettings(m_vrv);
      }
      else
      {
         m_scale_factor += wheel/1000.0;
         if (m_scale_factor < 0.01)
            m_scale_factor = 0.01;
         wxString str = wxString::Format("%.0f", m_scale_factor*100.0);
         m_vrv->m_scale_factor_sldr->SetValue(m_scale_factor*100);
         m_vrv->m_scale_factor_text->ChangeValue(str);
      }

      RefreshGL();
      return;
   }

   // draw the strokes into a framebuffer texture
   //not actually for displaying it
   if (m_draw_brush)
   {
      old_mouse_X = event.GetX();
      old_mouse_Y = event.GetY();
      RefreshGLOverlays();
      return;
   }

   if (m_draw_coord)
   {
      m_drawing_coord = true;
      RefreshGL(false, false);
      return;
   }
}

void VRenderGLView::SetDraw(bool draw)
{
   m_draw_all = draw;
}

void VRenderGLView::ToggleDraw()
{
   m_draw_all = !m_draw_all;
}

bool VRenderGLView::GetDraw()
{
   return m_draw_all;
}

Color VRenderGLView::GetBackgroundColor()
{
   return m_bg_color;
}

void VRenderGLView::SetBackgroundColor(Color &color)
{
   m_bg_color = color;
   HSVColor bg_color(m_bg_color);
   double hue, sat, val;
   if (bg_color.val()>0.7 && bg_color.sat()>0.7)
   {
      hue = bg_color.hue() + 180.0;
      sat = 1.0;
      val = 1.0;
      m_bg_color_inv = Color(HSVColor(hue, sat, val));
   }
   else if (bg_color.val()>0.7)
   {
      m_bg_color_inv = Color(0.0, 0.0, 0.0);
   }
   else
   {
      m_bg_color_inv = Color(1.0, 1.0, 1.0);
   }
}

void VRenderGLView::SetGradBg(bool val)
{
   m_grad_bg = val;
}

void VRenderGLView::SetRotations(double rotx, double roty, double rotz, bool ui_update, bool link_update)
{
   m_rotx = rotx;
   m_roty = roty;
   m_rotz = rotz;

   if (m_roty>360.0)
      m_roty -= 360.0;
   if (m_roty<0.0)
      m_roty += 360.0;
   if (m_rotx>360.0)
      m_rotx -= 360.0;
   if (m_rotx<0.0)
      m_rotx += 360.0;
   if (m_rotz>360.0)
      m_rotz -= 360.0;
   if (m_rotz<0.0)
      m_rotz += 360.0;

   A2Q();

   Quaternion cam_pos(0.0, 0.0, m_distance, 0.0);
   Quaternion cam_pos2 = (-m_q) * cam_pos * m_q;
   m_transx = cam_pos2.x;
   m_transy = cam_pos2.y;
   m_transz = cam_pos2.z;

   Quaternion up(0.0, 1.0, 0.0, 0.0);
   Quaternion up2 = (-m_q) * up * m_q;
   m_up = Vector(up2.x, up2.y, up2.z);

   if (m_rot_lock)
   {
      m_vrv->m_x_rot_sldr->SetValue(int(m_rotx/45.0));
      m_vrv->m_y_rot_sldr->SetValue(int(m_roty/45.0));
      m_vrv->m_z_rot_sldr->SetValue(int(m_rotz/45.0));
   }
   else
   {
      m_vrv->m_x_rot_sldr->SetValue(int(m_rotx));
      m_vrv->m_y_rot_sldr->SetValue(int(m_roty));
      m_vrv->m_z_rot_sldr->SetValue(int(m_rotz));
   }

   if (ui_update)
   {
      wxString str = wxString::Format("%.1f", m_rotx);
      m_vrv->m_x_rot_text->ChangeValue(str);
      str = wxString::Format("%.1f", m_roty);
      m_vrv->m_y_rot_text->ChangeValue(str);
      str = wxString::Format("%.1f", m_rotz);
      m_vrv->m_z_rot_text->ChangeValue(str);
   }

   if (m_linked_rot && link_update)
   {
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame)
      {
         for (int i=0; i<vr_frame->GetViewNum(); i++)
         {
            VRenderView* view = vr_frame->GetView(i);
            if (view && view->m_glview &&
                  view->m_glview!= this &&
                  view->m_glview->m_linked_rot)
            {
               view->m_glview->SetRotations(m_rotx, m_roty, m_rotz, true, false);
               view->RefreshGL();
            }
         }
      }
   }
}

char* VRenderGLView::wxStringToChar(wxString input)
{
#if (wxUSE_UNICODE)
   size_t size = input.size() + 1;
   char *buffer = new char[size];//No need to multiply by 4, converting to 1 byte char only.
   memset(buffer, 0, size); //Good Practice, Can use buffer[0] = '&#65533;' also.
   wxEncodingConverter wxec;
   wxec.Init(wxFONTENCODING_ISO8859_1, wxFONTENCODING_ISO8859_1, wxCONVERT_SUBSTITUTE);
   wxec.Convert(input.mb_str(), buffer);
   return buffer; //To free this buffer memory is user responsibility.
#else
   return (char *)(input.c_str());
#endif
}

void VRenderGLView::GetFrame(int &x, int &y, int &w, int &h)
{
   x = m_frame_x;
   y = m_frame_y;
   w = m_frame_w;
   h = m_frame_h;
}

void VRenderGLView::StartManipulation(Point *view_trans, Point *view_center, Point *view_rot, Point *obj_trans, double *scale)
{
	if(m_capture) return;

	if(!m_vrv)
		return;

	VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
	if (!vr_frame)
		return;
		
	DataManager* mgr = vr_frame->GetDataManager();
	if (!mgr)
		return;

	m_saved_uptime = TextureRenderer::get_up_time();
	TextureRenderer::set_up_time(m_manip_time);
	
	m_manip_interpolator.Clear();

	SetManipKey(0, 1, NULL, NULL, NULL, NULL, NULL); 

	double t = m_manip_time * m_manip_fps * 0.001;

	SetManipKey(t, 1, view_trans, view_center, view_rot, obj_trans, scale); 

	m_manip = true;
	m_manip_cur_num = 0;
	m_manip_end_frame = t;
}

void VRenderGLView::EndManipulation()
{
	TextureRenderer::set_up_time(m_saved_uptime);
	m_manip_interpolator.Clear();

	m_manip = false;
	m_manip_cur_num = 0;
	TextureRenderer::set_done_update_loop(true);
}

void VRenderGLView::SetManipKey(double t, int interpolation, Point *view_trans, Point *view_center, Point *view_rot, Point *obj_trans, double *scale)
{
	if(!m_vrv)
		return;

	VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
	if (!vr_frame)
		return;
		
	DataManager* mgr = vr_frame->GetDataManager();
	if (!mgr)
		return;

	ClippingView* clip_view = vr_frame->GetClippingView();
	
	KeyCode keycode;
	FlKeyDouble* flkey = 0;
	FlKeyQuaternion* flkeyQ = 0;
	FlKeyBoolean* flkeyB = 0;

	m_manip_interpolator.Begin(t);

	//for all volumes
	for (int i=0; i<mgr->GetVolumeNum() ; i++)
	{
		VolumeData* vd = mgr->GetVolumeData(i);
		keycode.l0 = 1;
		keycode.l0_name = m_vrv->GetName();
		keycode.l1 = 2;
		keycode.l1_name = vd->GetName();
		//display
		keycode.l2 = 0;
		keycode.l2_name = "display";
		flkeyB = new FlKeyBoolean(keycode, vd->GetDisp());
		m_manip_interpolator.AddKey(flkeyB);
		//clipping planes
		vector<Plane*> * planes = vd->GetVR()->get_planes();
		if (!planes)
			continue;
		if (planes->size() != 6)
			continue;
		Plane* plane = 0;
		double abcd[4];
		//x1
		plane = (*planes)[0];
		plane->get_copy(abcd);
		keycode.l2 = 0;
		keycode.l2_name = "x1_val";
		flkey = new FlKeyDouble(keycode, abs(abcd[3]));
		m_manip_interpolator.AddKey(flkey);
		//x2
		plane = (*planes)[1];
		plane->get_copy(abcd);
		keycode.l2 = 0;
		keycode.l2_name = "x2_val";
		flkey = new FlKeyDouble(keycode, abs(abcd[3]));
		m_manip_interpolator.AddKey(flkey);
		//y1
		plane = (*planes)[2];
		plane->get_copy(abcd);
		keycode.l2 = 0;
		keycode.l2_name = "y1_val";
		flkey = new FlKeyDouble(keycode, abs(abcd[3]));
		m_manip_interpolator.AddKey(flkey);
		//y2
		plane = (*planes)[3];
		plane->get_copy(abcd);
		keycode.l2 = 0;
		keycode.l2_name = "y2_val";
		flkey = new FlKeyDouble(keycode, abs(abcd[3]));
		m_manip_interpolator.AddKey(flkey);
		//z1
		plane = (*planes)[4];
		plane->get_copy(abcd);
		keycode.l2 = 0;
		keycode.l2_name = "z1_val";
		flkey = new FlKeyDouble(keycode, abs(abcd[3]));
		m_manip_interpolator.AddKey(flkey);
		//z2
		plane = (*planes)[5];
		plane->get_copy(abcd);
		keycode.l2 = 0;
		keycode.l2_name = "z2_val";
		flkey = new FlKeyDouble(keycode, abs(abcd[3]));
		m_manip_interpolator.AddKey(flkey);
	}
	//for the view
	keycode.l0 = 1;
	keycode.l0_name = m_vrv->GetName();
	keycode.l1 = 1;
	keycode.l1_name = m_vrv->GetName();
	//rotation
	keycode.l2 = 0;
	keycode.l2_name = "rotation";
	double rotx, roty, rotz;
	if (!view_rot) GetRotations(rotx, roty, rotz);
	else { rotx = view_rot->x(); roty = view_rot->y(); rotz = view_rot->z(); }
	Quaternion q;
	q.FromEuler(rotx, roty, rotz);
	flkeyQ = new FlKeyQuaternion(keycode, q);
	m_manip_interpolator.AddKey(flkeyQ);
	//translation
	double tx, ty, tz;
	if (!view_trans) GetTranslations(tx, ty, tz);
	else { tx = view_trans->x(); ty = view_trans->y(); tz = view_trans->z(); }
	//x
	keycode.l2_name = "translation_x";
	flkey = new FlKeyDouble(keycode, tx);
	m_manip_interpolator.AddKey(flkey);
	//y
	keycode.l2_name = "translation_y";
	flkey = new FlKeyDouble(keycode, ty);
	m_manip_interpolator.AddKey(flkey);
	//z
	keycode.l2_name = "translation_z";
	flkey = new FlKeyDouble(keycode, tz);
	m_manip_interpolator.AddKey(flkey);
	//centers
	if (!view_center) GetCenters(tx, ty, tz);
	else { tx = view_center->x(); ty = view_center->y(); tz = view_center->z(); }
	//x
	keycode.l2_name = "center_x";
	flkey = new FlKeyDouble(keycode, tx);
	m_manip_interpolator.AddKey(flkey);
	//y
	keycode.l2_name = "center_y";
	flkey = new FlKeyDouble(keycode, ty);
	m_manip_interpolator.AddKey(flkey);
	//z
	keycode.l2_name = "center_z";
	flkey = new FlKeyDouble(keycode, tz);
	m_manip_interpolator.AddKey(flkey);
	//obj traslation
	if (!obj_trans) GetObjTrans(tx, ty, tz);
	else { tx = obj_trans->x(); ty = obj_trans->y(); tz = obj_trans->z(); }
	//x
	keycode.l2_name = "obj_trans_x";
	flkey = new FlKeyDouble(keycode, tx);
	m_manip_interpolator.AddKey(flkey);
	//y
	keycode.l2_name = "obj_trans_y";
	flkey = new FlKeyDouble(keycode, ty);
	m_manip_interpolator.AddKey(flkey);
	//z
	keycode.l2_name = "obj_trans_z";
	flkey = new FlKeyDouble(keycode, tz);
	m_manip_interpolator.AddKey(flkey);
	//scale
	double scale_factor = scale ? *scale : m_scale_factor;
	keycode.l2_name = "scale";
	flkey = new FlKeyDouble(keycode, scale_factor);
	m_manip_interpolator.AddKey(flkey);

	m_manip_interpolator.End();

	FlKeyGroup* group = m_manip_interpolator.GetKeyGroup(m_manip_interpolator.GetLastIndex());
	if (group)
		group->type = interpolation;
	
}

void VRenderGLView::SetManipParams(double t)
{
	if (!m_vrv)
		return;
	VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
	if (!vr_frame)
		return;
	ClippingView* clip_view = vr_frame->GetClippingView();
	KeyCode keycode;
	keycode.l0 = 1;
	keycode.l0_name = m_vrv->GetName();

	for (int i=0; i<GetAllVolumeNum(); i++)
	{
		VolumeData* vd = GetAllVolumeData(i);
		if (!vd) continue;

		keycode.l1 = 2;
		keycode.l1_name = vd->GetName();

		//display
		keycode.l2 = 0;
		keycode.l2_name = "display";
		bool bval;
		if (m_manip_interpolator.GetBoolean(keycode, t, bval))
			vd->SetDisp(bval);

		//clipping planes
		vector<Plane*> *planes = vd->GetVR()->get_planes();
		if (!planes) continue;
		if(planes->size() != 6) continue;
		Plane *plane = 0;
		double val = 0;
		//x1
		plane = (*planes)[0];
		keycode.l2 = 0;
		keycode.l2_name = "x1_val";
		if (m_manip_interpolator.GetDouble(keycode, t, val))
			plane->ChangePlane(Point(abs(val), 0.0, 0.0),
			Vector(1.0, 0.0, 0.0));
		//x2
		plane = (*planes)[1];
		keycode.l2 = 0;
		keycode.l2_name = "x2_val";
		if (m_manip_interpolator.GetDouble(keycode, t, val))
			plane->ChangePlane(Point(abs(val), 0.0, 0.0),
			Vector(-1.0, 0.0, 0.0));
		//y1
		plane = (*planes)[2];
		keycode.l2 = 0;
		keycode.l2_name = "y1_val";
		if (m_manip_interpolator.GetDouble(keycode, t, val))
			plane->ChangePlane(Point(0.0, abs(val), 0.0),
			Vector(0.0, 1.0, 0.0));
		//y2
		plane = (*planes)[3];
		keycode.l2 = 0;
		keycode.l2_name = "y2_val";
		if (m_manip_interpolator.GetDouble(keycode, t, val))
			plane->ChangePlane(Point(0.0, abs(val), 0.0),
			Vector(0.0, -1.0, 0.0));
		//z1
		plane = (*planes)[4];
		keycode.l2 = 0;
		keycode.l2_name = "z1_val";
		if (m_manip_interpolator.GetDouble(keycode, t, val))
			plane->ChangePlane(Point(0.0, 0.0, abs(val)),
			Vector(0.0, 0.0, 1.0));
		//z2
		plane = (*planes)[5];
		keycode.l2 = 0;
		keycode.l2_name = "z2_val";
		if (m_manip_interpolator.GetDouble(keycode, t, val))
			plane->ChangePlane(Point(0.0, 0.0, abs(val)),
			Vector(0.0, 0.0, -1.0));
	}

	bool bx, by, bz;
	//for the view
	keycode.l1 = 1;
	keycode.l1_name = m_vrv->GetName();
	//translation
	double tx, ty, tz;
	keycode.l2 = 0;
	keycode.l2_name = "translation_x";
	bx = m_manip_interpolator.GetDouble(keycode, t, tx);
	keycode.l2_name = "translation_y";
	by = m_manip_interpolator.GetDouble(keycode, t, ty);
	keycode.l2_name = "translation_z";
	bz = m_manip_interpolator.GetDouble(keycode, t, tz);
	if (bx && by && bz)
		SetTranslations(tx, ty, tz);
	//centers
	keycode.l2_name = "center_x";
	bx = m_manip_interpolator.GetDouble(keycode, t, tx);
	keycode.l2_name = "center_y";
	by = m_manip_interpolator.GetDouble(keycode, t, ty);
	keycode.l2_name = "center_z";
	bz = m_manip_interpolator.GetDouble(keycode, t, tz);
	if (bx && by && bz)
		SetCenters(tx, ty, tz);
	//obj translation
	keycode.l2_name = "obj_trans_x";
	bx = m_manip_interpolator.GetDouble(keycode, t, tx);
	keycode.l2_name = "obj_trans_y";
	by = m_manip_interpolator.GetDouble(keycode, t, ty);
	keycode.l2_name = "obj_trans_z";
	bz = m_manip_interpolator.GetDouble(keycode, t, tz);
	if (bx && by && bz)
		SetObjTrans(tx, ty, tz);
	//scale
	double scale;
	keycode.l2_name = "scale";
	if (m_manip_interpolator.GetDouble(keycode, t, scale))
		m_vrv->SetScaleFactor(scale);
	//rotation
	keycode.l2 = 0;
	keycode.l2_name = "rotation";
	Quaternion q;
	if (m_manip_interpolator.GetQuaternion(keycode, t, q))
	{
		double rotx, roty, rotz;
		q.ToEuler(rotx, roty, rotz);
		SetRotations(rotx, roty, rotz, true);
	}

	if (clip_view)
		clip_view->SetVolumeData(vr_frame->GetCurSelVol());
	if (vr_frame)
	{
		vr_frame->UpdateTree();
		int index = m_manip_interpolator.GetKeyIndexFromTime(t);
		vr_frame->GetRecorderDlg()->SetSelection(index);
	}
	SetVolPopDirty();
}

void VRenderGLView::DrawViewQuad()
{
	glEnable(GL_TEXTURE_2D);
	if (!glIsVertexArray(m_quad_vao))
	{
		float points[] = {
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			-1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
			1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};

		glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*24, points, GL_STATIC_DRAW);

		glGenVertexArrays(1, &m_quad_vao);
		glBindVertexArray(m_quad_vao);

		glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (const GLvoid*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (const GLvoid*)12);
	}

	glBindVertexArray(m_quad_vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
	glDisable(GL_TEXTURE_2D);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(VRenderView, wxPanel)
//bar top
EVT_RADIOBUTTON(ID_VolumeSeqRd, VRenderView::OnVolumeMethodCheck)
EVT_RADIOBUTTON(ID_VolumeMultiRd, VRenderView::OnVolumeMethodCheck)
EVT_RADIOBUTTON(ID_VolumeCompRd, VRenderView::OnVolumeMethodCheck)
EVT_BUTTON(ID_CaptureBtn, VRenderView::OnCapture)
EVT_COLOURPICKER_CHANGED(ID_BgColorPicker, VRenderView::OnBgColorChange)
EVT_CHECKBOX(ID_CamCtrChk, VRenderView::OnCamCtrCheck)
EVT_CHECKBOX(ID_FpsChk, VRenderView::OnFpsCheck)
EVT_CHECKBOX(ID_LegendChk, VRenderView::OnLegendCheck)
EVT_CHECKBOX(ID_IntpChk, VRenderView::OnIntpCheck)
EVT_CHECKBOX(ID_SearchChk, VRenderView::OnSearchCheck)
EVT_COMMAND_SCROLL(ID_AovSldr, VRenderView::OnAovChange)
EVT_TEXT(ID_AovText, VRenderView::OnAovText)
EVT_CHECKBOX(ID_FreeChk, VRenderView::OnFreeChk)
EVT_COMMAND_SCROLL(ID_PPISldr, VRenderView::OnPPIChange)
EVT_TEXT(ID_PPIText, VRenderView::OnPPIEdit)
//bar left
EVT_CHECKBOX(ID_DepthAttenChk, VRenderView::OnDepthAttenCheck)
EVT_COMMAND_SCROLL(ID_DepthAttenFactorSldr, VRenderView::OnDepthAttenFactorChange)
EVT_TEXT(ID_DepthAttenFactorText, VRenderView::OnDepthAttenFactorEdit)
EVT_BUTTON(ID_DepthAttenResetBtn, VRenderView::OnDepthAttenReset)
//bar right
EVT_BUTTON(ID_CenterBtn, VRenderView::OnCenter)
EVT_BUTTON(ID_Scale121Btn, VRenderView::OnScale121)
EVT_COMMAND_SCROLL(ID_ScaleFactorSldr, VRenderView::OnScaleFactorChange)
EVT_TEXT(ID_ScaleFactorText, VRenderView::OnScaleFactorEdit)
EVT_BUTTON(ID_ScaleResetBtn, VRenderView::OnScaleReset)
EVT_SPIN_UP(ID_ScaleFactorSpin, VRenderView::OnScaleFactorSpinDown)
EVT_SPIN_DOWN(ID_ScaleFactorSpin, VRenderView::OnScaleFactorSpinUp)
//bar bottom
EVT_CHECKBOX(ID_RotLinkChk, VRenderView::OnRotLink)
EVT_BUTTON(ID_RotResetBtn, VRenderView::OnRotReset)
EVT_TEXT(ID_XRotText, VRenderView::OnValueEdit)
EVT_TEXT(ID_YRotText, VRenderView::OnValueEdit)
EVT_TEXT(ID_ZRotText, VRenderView::OnValueEdit)
EVT_COMMAND_SCROLL(ID_XRotSldr, VRenderView::OnXRotScroll)
EVT_COMMAND_SCROLL(ID_YRotSldr, VRenderView::OnYRotScroll)
EVT_COMMAND_SCROLL(ID_ZRotSldr, VRenderView::OnZRotScroll)
EVT_CHECKBOX(ID_RotLockChk, VRenderView::OnRotLockCheck)
//spin buttons
EVT_SPIN_UP(ID_XRotSpin, VRenderView::OnXRotSpinUp)
EVT_SPIN_DOWN(ID_XRotSpin, VRenderView::OnXRotSpinDown)
EVT_SPIN_UP(ID_YRotSpin, VRenderView::OnYRotSpinUp)
EVT_SPIN_DOWN(ID_YRotSpin, VRenderView::OnYRotSpinDown)
EVT_SPIN_UP(ID_ZRotSpin, VRenderView::OnZRotSpinUp)
EVT_SPIN_DOWN(ID_ZRotSpin, VRenderView::OnZRotSpinDown)
//reset
EVT_BUTTON(ID_DefaultBtn, VRenderView::OnSaveDefault)

EVT_KEY_DOWN(VRenderView::OnKeyDown)
END_EVENT_TABLE()

VRenderView::VRenderView(wxWindow* frame,
      wxWindow* parent,
      wxWindowID id,
      wxGLContext* sharedContext,
      const wxPoint& pos,
      const wxSize& size,
      long style) :
   wxPanel(parent, id, pos, size, style),
   m_frame(frame),
   m_draw_clip(false),
   m_use_dft_settings(false)
{
   wxString name = wxString::Format("Render View:%d", m_id++);
   this->SetName(name);

   //render view/////////////////////////////////////////////////
   m_glview = new VRenderGLView(frame, this, wxID_ANY, sharedContext);
   m_glview->SetCanFocus(false);

   CreateBar();
   LoadSettings();
}

VRenderView::~VRenderView()
{
   if (m_glview)
      delete m_glview;
}

void VRenderView::CreateBar()
{
   //validator: floating point 1
   wxFloatingPointValidator<double> vald_fp1(1);
   vald_fp1.SetRange(0.0, 360.0);
   //validator: floating point 2
   wxFloatingPointValidator<double> vald_fp2(2);
   vald_fp2.SetRange(0.0, 1.0);
   //validator: integer
   wxIntegerValidator<unsigned int> vald_int;
   vald_int.SetMin(1);

   wxBoxSizer* sizer_v = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer* sizer_m = new wxBoxSizer(wxHORIZONTAL);
   wxStaticText *st1, *st2, *st3, *st4;

   //bar top///////////////////////////////////////////////////
   wxBoxSizer* sizer_h_1 = new wxBoxSizer(wxHORIZONTAL);
   m_volume_seq_rd = new wxRadioButton(this, ID_VolumeSeqRd, "Layered",
         wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
   m_volume_multi_rd = new wxRadioButton(this, ID_VolumeMultiRd, "Depth",
         wxDefaultPosition, wxDefaultSize);
   m_volume_comp_rd = new wxRadioButton(this, ID_VolumeCompRd, "Composite",
         wxDefaultPosition, wxDefaultSize);
   m_volume_seq_rd->SetValue(false);
   m_volume_multi_rd->SetValue(false);
   m_volume_comp_rd->SetValue(false);
   switch (m_glview->GetVolMethod())
   {
   case VOL_METHOD_SEQ:
      m_volume_seq_rd->SetValue(true);
      break;
   case VOL_METHOD_MULTI:
      m_volume_multi_rd->SetValue(true);
      break;
   case VOL_METHOD_COMP:
      m_volume_comp_rd->SetValue(true);
      break;
   }
   m_capture_btn = new wxButton(this, ID_CaptureBtn, "Capture",
         wxDefaultPosition, wxSize(80, 20));
   st1 = new wxStaticText(this, 0, "Background:");
   m_bg_color_picker = new wxColourPickerCtrl(this, ID_BgColorPicker, *wxBLACK,
         wxDefaultPosition, wxDefaultSize/*wxSize(40, 20)*/);
   m_cam_ctr_chk = new wxCheckBox(this, ID_CamCtrChk, "Center",
         wxDefaultPosition, wxSize(-1, 20));
   m_cam_ctr_chk->SetValue(false);
   m_fps_chk = new wxCheckBox(this, ID_FpsChk, "Info.",
         wxDefaultPosition, wxSize(-1, 20));
   m_fps_chk->SetValue(false);
   m_legend_chk = new wxCheckBox(this, ID_LegendChk, "Legend",
         wxDefaultPosition, wxSize(-1, 20));
   m_legend_chk->SetValue(false);
   m_intp_chk = new wxCheckBox(this, ID_IntpChk, "Intrp.",
         wxDefaultPosition, wxSize(-1, 20));
   m_intp_chk->SetValue(true);
   m_search_chk = new wxCheckBox(this, ID_SearchChk, "Search",
	   wxDefaultPosition, wxSize(-1, 20));
   m_search_chk->SetValue(false);
#ifndef WITH_DATABASE
   m_search_chk->Hide();
#endif // !WITH_DATABASE
   //angle of view
   st2 = new wxStaticText(this, 0, "V. AOV:");
   m_aov_sldr = new wxSlider(this, ID_AovSldr, 45, 10, 100,
         wxDefaultPosition, wxSize(120, 20), wxSL_HORIZONTAL);
   m_aov_sldr->SetValue(GetPersp()?GetAov():10);
   m_aov_sldr->Connect(wxID_ANY, wxEVT_IDLE,
         wxIdleEventHandler(VRenderView::OnAovSldrIdle),
         NULL, this);
   m_aov_text = new wxTextCtrl(this, ID_AovText, "",
         wxDefaultPosition, wxSize(60, 20), 0, vald_int);
   m_aov_text->ChangeValue(GetPersp()?wxString::Format("%d", int(GetAov())):"Ortho");
   m_free_chk = new wxCheckBox(this, ID_FreeChk, "FreeFly",
         wxDefaultPosition, wxSize(-1, 20));
   if (GetFree())
      m_free_chk->SetValue(true);
   else
      m_free_chk->SetValue(false);
   
   st3 = new wxStaticText(this, 0, "Min dpi:");
   m_ppi_sldr = new wxSlider(this, ID_PPISldr, 20, 5, 100,
         wxDefaultPosition, wxSize(120, 20), wxSL_HORIZONTAL);
   m_ppi_text = new wxTextCtrl(this, ID_PPIText, "20",
         wxDefaultPosition, wxSize(40, 20), 0, vald_int);
   
   //
   sizer_h_1->Add(10, 5, 0);
   sizer_h_1->Add(m_volume_seq_rd, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(m_volume_multi_rd, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(m_volume_comp_rd, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(10, 5, 0);
   sizer_h_1->Add(m_capture_btn, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(10, 5, 0);
   sizer_h_1->Add(st1, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(m_bg_color_picker, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(10, 5, 0);
   sizer_h_1->Add(m_cam_ctr_chk, 0, wxALIGN_CENTER);
   sizer_h_1->Add(5, 5, 0);
   sizer_h_1->Add(m_fps_chk, 0, wxALIGN_CENTER);
   sizer_h_1->Add(5, 5, 0);
   sizer_h_1->Add(m_legend_chk, 0, wxALIGN_CENTER);
   sizer_h_1->Add(5, 5, 0);
   sizer_h_1->Add(m_intp_chk, 0, wxALIGN_CENTER);
   sizer_h_1->Add(5, 5, 0);
   sizer_h_1->Add(m_search_chk, 0, wxALIGN_CENTER);
   sizer_h_1->Add(5, 5, 0);
   sizer_h_1->Add(st2, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(m_aov_sldr, 0, wxALIGN_CENTER);
   sizer_h_1->Add(m_aov_text, 0, wxALIGN_CENTER);
   sizer_h_1->Add(5, 5, 0);
   sizer_h_1->Add(m_free_chk, 0, wxALIGN_CENTER);
   sizer_h_1->Add(5, 5, 0);
   sizer_h_1->Add(st3, 0, wxALIGN_CENTER, 0);
   sizer_h_1->Add(m_ppi_sldr, 0, wxALIGN_CENTER);
   sizer_h_1->Add(m_ppi_text, 0, wxALIGN_CENTER);
   
   //bar left///////////////////////////////////////////////////
   wxBoxSizer* sizer_v_3 = new wxBoxSizer(wxVERTICAL);
   st1 = new wxStaticText(this, 0, "Depth\nInt:\n");
   m_depth_atten_chk = new wxCheckBox(this, ID_DepthAttenChk, "");
   m_depth_atten_chk->SetValue(true);
   m_depth_atten_factor_sldr = new wxSlider(this, ID_DepthAttenFactorSldr, 0, 0, 100,
         wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL|wxSL_INVERSE);
   m_depth_atten_reset_btn = new wxButton(this, ID_DepthAttenResetBtn, "Reset",
         wxDefaultPosition, wxSize(60, 20));
   m_depth_atten_factor_text = new wxTextCtrl(this, ID_DepthAttenFactorText, "0.0",
         wxDefaultPosition, wxSize(40, 20), 0, vald_fp2);
   sizer_v_3->Add(5, 10, 0);
   sizer_v_3->Add(st1, 0, wxALIGN_CENTER);
   sizer_v_3->Add(m_depth_atten_chk, 0, wxALIGN_CENTER);
   sizer_v_3->Add(m_depth_atten_factor_sldr, 1, wxEXPAND|wxALIGN_CENTER);
   sizer_v_3->Add(m_depth_atten_reset_btn, 0, wxALIGN_CENTER);
   sizer_v_3->Add(m_depth_atten_factor_text, 0, wxALIGN_CENTER);

   //bar right///////////////////////////////////////////////////
   wxBoxSizer* sizer_v_4 = new wxBoxSizer(wxVERTICAL);
   st1 = new wxStaticText(this, 0, "Zoom:\n");
   m_center_btn = new wxButton(this, ID_CenterBtn, "Center",
         wxDefaultPosition, wxSize(65, 20));
   m_scale_121_btn = new wxButton(this, ID_Scale121Btn, "1:1",
         wxDefaultPosition, wxSize(40, 20));
   m_scale_factor_sldr = new wxSlider(this, ID_ScaleFactorSldr, 100, 50, 999,
         wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL);
   m_scale_reset_btn = new wxButton(this, ID_ScaleResetBtn, "Reset",
         wxDefaultPosition, wxSize(60, 20));
   m_scale_factor_text = new wxTextCtrl(this, ID_ScaleFactorText, "100",
         wxDefaultPosition, wxSize(40, 20), 0, vald_int);
   m_scale_factor_spin = new wxSpinButton(this, ID_ScaleFactorSpin,
         wxDefaultPosition, wxSize(40, 20));
   sizer_v_4->Add(5, 10, 0);
   sizer_v_4->Add(st1, 0, wxALIGN_CENTER);
   sizer_v_4->Add(m_center_btn, 0, wxALIGN_CENTER);
   sizer_v_4->Add(m_scale_121_btn, 0, wxALIGN_CENTER);
   sizer_v_4->Add(m_scale_factor_sldr, 1, wxEXPAND|wxALIGN_CENTER);
   sizer_v_4->Add(m_scale_factor_spin, 0, wxALIGN_CENTER);
   sizer_v_4->Add(m_scale_reset_btn, 0, wxALIGN_CENTER);
   sizer_v_4->Add(m_scale_factor_text, 0, wxALIGN_CENTER);

   //middle sizer
   sizer_m->Add(sizer_v_3, 0, wxEXPAND);
   sizer_m->Add(m_glview, 1, wxEXPAND);
   sizer_m->Add(sizer_v_4, 0, wxEXPAND);

   //bar bottom///////////////////////////////////////////////////
   wxBoxSizer* sizer_h_2 = new wxBoxSizer(wxHORIZONTAL);
   m_rot_link_chk = new wxCheckBox(this, ID_RotLinkChk, "Link");
   m_rot_reset_btn = new wxButton(this, ID_RotResetBtn, "Reset to 0",
         wxDefaultPosition, wxSize(85, 20));
   st1 = new wxStaticText(this, 0, "X:");
   m_x_rot_sldr = new wxSlider(this, ID_XRotSldr, 0, 0, 360,
         wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
   m_x_rot_text = new wxTextCtrl(this, ID_XRotText, "0.0",
         wxDefaultPosition, wxSize(60,20), 0, vald_fp1);
   m_x_rot_spin = new wxSpinButton(this, ID_XRotSpin,
         wxDefaultPosition, wxSize(20, 20), wxSP_HORIZONTAL);
   st2 = new wxStaticText(this, 0, "Y:");
   m_y_rot_sldr = new wxSlider(this, ID_YRotSldr, 0, 0, 360,
         wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
   m_y_rot_text = new wxTextCtrl(this, ID_YRotText, "0.0",
         wxDefaultPosition, wxSize(60,20), 0, vald_fp1);
   m_y_rot_spin = new wxSpinButton(this, ID_YRotSpin,
         wxDefaultPosition, wxSize(20, 20), wxSP_HORIZONTAL);
   st3 = new wxStaticText(this, 0, "Z:");
   m_z_rot_sldr = new wxSlider(this, ID_ZRotSldr, 0, 0, 360,
         wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
   m_z_rot_text = new wxTextCtrl(this, ID_ZRotText, "0.0",
         wxDefaultPosition, wxSize(60,20), 0, vald_fp1);
   m_z_rot_spin = new wxSpinButton(this, ID_ZRotSpin,
         wxDefaultPosition, wxSize(20, 20), wxSP_HORIZONTAL);
   m_rot_lock_chk = new wxCheckBox(this, ID_RotLockChk, "45 Increments");
   m_default_btn = new wxButton(this, ID_DefaultBtn, "Save as Default",
         wxDefaultPosition, wxSize(115, 20));
   sizer_h_2->Add(m_rot_link_chk, 0, wxALIGN_CENTER);
   sizer_h_2->Add(m_rot_reset_btn, 0, wxALIGN_CENTER);
   sizer_h_2->Add(5, 5, 0);
   sizer_h_2->Add(st1, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_x_rot_sldr, 1, wxEXPAND|wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_x_rot_spin, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_x_rot_text, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(5, 5, 0);
   sizer_h_2->Add(st2, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_y_rot_sldr, 1, wxEXPAND|wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_y_rot_spin, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_y_rot_text, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(5, 5, 0);
   sizer_h_2->Add(st3, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_z_rot_sldr, 1, wxEXPAND|wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_z_rot_spin, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(m_z_rot_text, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(5, 5, 0);
   sizer_h_2->Add(m_rot_lock_chk, 0, wxALIGN_CENTER, 0);
   sizer_h_2->Add(5, 5, 0);
   sizer_h_2->Add(m_default_btn, 0, wxALIGN_CENTER);

   sizer_v->Add(sizer_h_1, 0, wxEXPAND);
   sizer_v->Add(sizer_m, 1, wxEXPAND);
   sizer_v->Add(sizer_h_2, 0, wxEXPAND);

   SetSizer(sizer_v);
   Layout();

}

//recalculate view according to object bounds
void VRenderView::InitView(unsigned int type)
{
   if (m_glview)
      m_glview->InitView(type);
   if (m_use_dft_settings)
      UpdateView();
}

void VRenderView::Clear()
{
   if (m_glview)
      m_glview->Clear();
   ClearVolList();
   ClearMeshList();
}

//data management
int VRenderView::GetDispVolumeNum()
{
   if (m_glview)
      return m_glview->GetDispVolumeNum();
   else
      return 0;
}

int VRenderView::GetAny()
{
   if (m_glview)
      return m_glview->GetAny();
   else
      return 0;
}

int VRenderView::GetAllVolumeNum()
{
   if (m_glview)
      return m_glview->GetAllVolumeNum();
   else return 0;
}

int VRenderView::GetMeshNum()
{
   if (m_glview)
      return m_glview->GetMeshNum();
   else
      return 0;
}

int VRenderView::GetGroupNum()
{
   if (m_glview)
      return m_glview->GetGroupNum();
   else
      return 0;
}

int VRenderView::GetLayerNum()
{
   if (m_glview)
      return m_glview->GetLayerNum();
   else
      return 0;
}

VolumeData* VRenderView::GetAllVolumeData(int index)
{
   if (m_glview)
      return m_glview->GetAllVolumeData(index);
   else
      return 0;
}

VolumeData* VRenderView::GetDispVolumeData(int index)
{
   if (m_glview)
      return m_glview->GetDispVolumeData(index);
   else
      return 0;
}

MeshData* VRenderView::GetMeshData(int index)
{
   if (m_glview)
      return m_glview->GetMeshData(index);
   else
      return 0;
}

TreeLayer* VRenderView::GetLayer(int index)
{
   if (m_glview)
      return m_glview->GetLayer(index);
   else
      return 0;
}

MultiVolumeRenderer* VRenderView::GetMultiVolumeData()
{
   if (m_glview)
      return m_glview->GetMultiVolumeData();
   else
      return 0;
}

VolumeData* VRenderView::GetVolumeData(wxString &name)
{
   if (m_glview)
      return m_glview->GetVolumeData(name);
   else
      return 0;
}

MeshData* VRenderView::GetMeshData(wxString &name)
{
   if (m_glview)
      return m_glview->GetMeshData(name);
   else
      return 0;
}

Annotations* VRenderView::GetAnnotations(wxString &name)
{
   if (m_glview)
      return m_glview->GetAnnotations(name);
   else
      return 0;
}

DataGroup* VRenderView::GetGroup(wxString &name)
{
   if (m_glview)
      return m_glview->GetGroup(name);
   else
      return 0;
}

DataGroup* VRenderView::AddVolumeData(VolumeData* vd, wxString group_name)
{
   if (m_glview)
      return m_glview->AddVolumeData(vd, group_name);
   else
      return 0;
}

void VRenderView::AddMeshData(MeshData* md)
{
   if (m_glview)
      m_glview->AddMeshData(md);
}

void VRenderView::AddAnnotations(Annotations* ann)
{
   if (m_glview)
      m_glview->AddAnnotations(ann);
}

wxString VRenderView::AddGroup(wxString str, wxString prev_group)
{
   if (m_glview)
      return m_glview->AddGroup(str);
   else
      return "";
}

wxString VRenderView::AddMGroup(wxString str)
{
   if (m_glview)
      return m_glview->AddMGroup(str);
   else
      return "";
}

MeshGroup* VRenderView::GetMGroup(wxString &str)
{
   if (m_glview)
      return m_glview->GetMGroup(str);
   else return 0;
}

void VRenderView::RemoveVolumeData(wxString &name)
{
   if (m_glview)
      m_glview->RemoveVolumeData(name);
}

void VRenderView::RemoveMeshData(wxString &name)
{
   if (m_glview)
      m_glview->RemoveMeshData(name);
}

void VRenderView::RemoveAnnotations(wxString &name)
{
   if (m_glview)
      m_glview->RemoveAnnotations(name);
}

void VRenderView::RemoveGroup(wxString &name)
{
   if (m_glview)
      m_glview->RemoveGroup(name);
}

void VRenderView::Isolate(int type, wxString name)
{
   if (m_glview)
      m_glview->Isolate(type, name);
}

//move
void VRenderView::MoveLayerinView(wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveLayerinView(src_name, dst_name);
}

//move volume
void VRenderView::MoveLayerinGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveLayerinGroup(group_name, src_name, dst_name);
}

void VRenderView::MoveLayertoView(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveLayertoView(group_name, src_name, dst_name);
}

void VRenderView::MoveLayertoGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveLayertoGroup(group_name, src_name, dst_name);
}

void VRenderView::MoveLayerfromtoGroup(wxString &src_group_name,
      wxString &dst_group_name,
      wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveLayerfromtoGroup(src_group_name,
            dst_group_name, src_name, dst_name);
}

//move mesh
void VRenderView::MoveMeshinGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveMeshinGroup(group_name, src_name, dst_name);
}

void VRenderView::MoveMeshtoView(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveMeshtoView(group_name, src_name, dst_name);
}

void VRenderView::MoveMeshtoGroup(wxString &group_name, wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveMeshtoGroup(group_name, src_name, dst_name);
}

void VRenderView::MoveMeshfromtoGroup(wxString &src_group_name,
      wxString &dst_group_name,
      wxString &src_name, wxString &dst_name)
{
   if (m_glview)
      m_glview->MoveMeshfromtoGroup(src_group_name,
            dst_group_name, src_name, dst_name);
}

//reorganize layers in view
void VRenderView::OrganizeLayers()
{
   if (m_glview)
      m_glview->OrganizeLayers();
}

//randomize color
void VRenderView::RandomizeColor()
{
   if (m_glview)
      m_glview->RandomizeColor();
}

//toggle hiding/displaying
void VRenderView::SetDraw(bool draw)
{
   if (m_glview)
      m_glview->SetDraw(draw);
}

void VRenderView::ToggleDraw()
{
   if (m_glview)
      m_glview->ToggleDraw();
}

bool VRenderView::GetDraw()
{
   if (m_glview)
      return m_glview->GetDraw();
   else
      return false;
}

//camera operations
void VRenderView::GetTranslations(double &transx, double &transy, double &transz)
{
   if (m_glview)
      m_glview->GetTranslations(transx, transy, transz);
}

void VRenderView::SetTranslations(double transx, double transy, double transz)
{
   if (m_glview)
      m_glview->SetTranslations(transx, transy, transz);
}

void VRenderView::GetRotations(double &rotx, double &roty, double &rotz)
{
   if (m_glview)
      m_glview->GetRotations(rotx, roty, rotz);
}

void VRenderView::SetRotations(double rotx, double roty, double rotz, bool ui_update)
{
   if (m_glview)
      m_glview->SetRotations(rotx, roty, rotz, ui_update);

}

void VRenderView::GetCenters(double &ctrx, double &ctry, double &ctrz)
{
   if (m_glview)
      m_glview->GetCenters(ctrx, ctry, ctrz);
}

void VRenderView::SetCenters(double ctrx, double ctry, double ctrz)
{
   if (m_glview)
      m_glview->SetCenters(ctrx, ctry, ctrz);
}

double VRenderView::GetCenterEyeDist()
{
   if (m_glview)
      return m_glview->GetCenterEyeDist();
   else
      return 0.0;
}

void VRenderView::SetCenterEyeDist(double dist)
{
   if (m_glview)
      m_glview->SetCenterEyeDist(dist);
}

double VRenderView::GetRadius()
{
   if (m_glview)
      return m_glview->GetRadius();
   else
      return 0.0;
}

void VRenderView::SetRadius(double r)
{
   if (m_glview)
      m_glview->SetRadius(r);
}

//object operations
void VRenderView::GetObjCenters(double &ctrx, double &ctry, double &ctrz)
{
   if (m_glview)
      m_glview->GetObjCenters(ctrx, ctry, ctrz);
}

void VRenderView::SetObjCenters(double ctrx, double ctry, double ctrz)
{
   if (m_glview)
      m_glview->SetObjCenters(ctrx, ctry, ctrz);
}

void VRenderView::GetObjTrans(double &transx, double &transy, double &transz)
{
   if (m_glview)
      m_glview->GetObjTrans(transx, transy, transz);
}

void VRenderView::SetObjTrans(double transx, double transy, double transz)
{
   if (m_glview)
      m_glview->SetObjTrans(transx, transy, transz);
}

void VRenderView::GetObjRot(double &rotx, double &roty, double &rotz)
{
   if (m_glview)
      m_glview->GetObjRot(rotx, roty, rotz);
}

void VRenderView::SetObjRot(double rotx, double roty, double rotz)
{
   if (m_glview)
      m_glview->SetObjRot(rotx, roty, rotz);
}

//camera properties
double VRenderView::GetAov()
{
   if (m_glview)
      return m_glview->GetAov();
   else
      return 0.0;
}

void VRenderView::SetAov(double aov)
{
   if (m_glview)
      m_glview->SetAov(aov);
}

double VRenderView::GetMinPPI()
{
   if (m_glview)
      return m_glview->GetMinPPI();
   else
      return 20.0;
}
void VRenderView::SetMinPPI(double ppi)
{
   if (m_glview)
      m_glview->SetMinPPI(ppi);
}

double VRenderView::GetNearClip()
{
   if (m_glview)
      return m_glview->GetNearClip();
   else
      return 0.0;
}

void VRenderView::SetNearClip(double nc)
{
   if (m_glview)
      m_glview->SetNearClip(nc);
}

double VRenderView::GetFarClip()
{
   if (m_glview)
      return m_glview->GetFarClip();
   else
      return 0.0;
}

void VRenderView::SetFarClip(double fc)
{
   if (m_glview)
      m_glview->SetFarClip(fc);
}

//background color
Color VRenderView::GetBackgroundColor()
{
   if (m_glview)
      return m_glview->GetBackgroundColor();
   else
      return Color(0, 0, 0);
}

void VRenderView::SetBackgroundColor(Color &color)
{
   if (m_glview)
      m_glview->SetBackgroundColor(color);
   wxColor c(int(color.r()*255.0), int(color.g()*255.0), int(color.b()*255.0));
   m_bg_color_picker->SetColour(c);
}

void VRenderView::SetGradBg(bool val)
{
   if (m_glview)
      m_glview->SetGradBg(val);
}

//point volume mode
void VRenderView::SetPointVolumeMode(int mode)
{
   if (m_glview)
      m_glview->m_point_volume_mode = mode;
}

int VRenderView::GetPointVolumeMode()
{
   if (m_glview)
      return m_glview->m_point_volume_mode;
   else
      return 0;
}

//ruler uses trnasfer function
void VRenderView::SetRulerUseTransf(bool val)
{
   if (m_glview)
      m_glview->m_ruler_use_transf = val;
}

bool VRenderView::GetRulerUseTransf()
{
   if (m_glview)
      return m_glview->m_ruler_use_transf;
   else
      return false;
}

//ruler time dependent
void VRenderView::SetRulerTimeDep(bool val)
{
   if (m_glview)
      m_glview->m_ruler_time_dep = val;
}

bool VRenderView::GetRulerTimeDep()
{
   if (m_glview)
      return m_glview->m_ruler_time_dep;
   else
      return true;
}

//disply modes
int VRenderView::GetDrawType()
{
   if (m_glview)
      return m_glview->GetDrawType();
   else
      return 0;
}

void VRenderView::SetVolMethod(int method)
{
   if (m_glview)
      m_glview->SetVolMethod(method);
}

int VRenderView::GetVolMethod()
{
   if (m_glview)
      return m_glview->GetVolMethod();
   else
      return 0;
}

//other properties
void VRenderView::SetPeelingLayers(int n)
{
   if (m_glview)
      m_glview->SetPeelingLayers(n);
}

int VRenderView::GetPeelingLayers()
{
   if (m_glview)
      return m_glview->GetPeelingLayers();
   else
      return 0;
}

void VRenderView::SetBlendSlices(bool val)
{
   if (m_glview)
      m_glview->SetBlendSlices(val);
}

bool VRenderView::GetBlendSlices()
{
   if (m_glview)
      return m_glview->GetBlendSlices();
   else
      return false;
}

void VRenderView::SetAdaptive(bool val)
{
   if (m_glview)
      m_glview->SetAdaptive(val);
}

bool VRenderView::GetAdaptive()
{
   if (m_glview)
      return m_glview->GetAdaptive();
   else
      return false;
}

void VRenderView::SetFog(bool b)
{
   if (m_glview)
      m_glview->SetFog(b);
   if (m_depth_atten_chk)
      m_depth_atten_chk->SetValue(b);
}

bool VRenderView::GetFog()
{
   if (m_glview)
      return m_glview->GetFog();
   else
      return false;
}

void VRenderView::SetFogIntensity(double i)
{
   if (m_glview)
      m_glview->SetFogIntensity(i);
}

double VRenderView::GetFogIntensity()
{
   if (m_glview)
      return m_glview->GetFogIntensity();
   else
      return 0.0;
}

//reset counter
void VRenderView::ResetID()
{
   m_id = 1;
}

//get rendering context
wxGLContext* VRenderView::GetContext()
{
   if (m_glview)
      return m_glview->m_glRC;
   else
      return 0;
}

void VRenderView::RefreshGL(bool interactive)
{
   if (m_glview)
   {
      m_glview->m_force_clear = true;
	  if (m_glview->m_adaptive) m_glview->m_interactive = interactive;
	  if (m_glview->m_adaptive_res) m_glview->m_int_res = interactive;
      m_glview->RefreshGL();
   }
}

void VRenderView::RefreshGLOverlays()
{
   if (m_glview)
   {
      m_glview->RefreshGLOverlays();
   }
}

//bar top
void VRenderView::OnVolumeMethodCheck(wxCommandEvent& event)
{
   //mode switch type
   //0 - didn't change
   //1 - to depth mode
   //2 - from depth mode
   int mode_switch_type = 0;
   int old_mode = GetVolMethod();

   int sender_id = event.GetId();
   switch (sender_id)
   {
   case ID_VolumeSeqRd:
      SetVolMethod(VOL_METHOD_SEQ);
      break;
   case ID_VolumeMultiRd:
      SetVolMethod(VOL_METHOD_MULTI);
      break;
   case ID_VolumeCompRd:
      SetVolMethod(VOL_METHOD_COMP);
      break;
   }

   int new_mode = GetVolMethod();

   if (new_mode == VOL_METHOD_MULTI &&
         (old_mode == VOL_METHOD_SEQ || old_mode == VOL_METHOD_COMP))
      mode_switch_type = 1;
   else if ((new_mode == VOL_METHOD_SEQ || new_mode == VOL_METHOD_COMP) &&
         old_mode == VOL_METHOD_MULTI)
      mode_switch_type = 2;

   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
   if (vr_frame)
   {
      if (mode_switch_type == 1)
      {
         int cnt = 0;
         bool r = false;
         bool g = false;
         bool b = false;
         Color gamma = m_glview->GetGamma();
         Color brightness = m_glview->GetBrightness();
         Color hdr = m_glview->GetHdr();
         for (int i=0; i<GetLayerNum(); i++)
         {
            TreeLayer* layer = GetLayer(i);
            if (layer && layer->IsA() == 5)
            {
               DataGroup* group = (DataGroup*)layer;

               if (group->GetVolumeNum() == 0)
                  continue;

               r = r||group->GetSyncR();
               g = g||group->GetSyncG();
               b = b||group->GetSyncB();

               if (cnt == 0)
               {
                  gamma = group->GetGamma();
                  brightness = group->GetBrightness();
                  hdr = group->GetHdr();
               }
            }
            cnt++;
         }

         if (r && g && b)
         {
            gamma[1] = gamma[2] = gamma[0];
            brightness[1] = brightness[2] = brightness[0];
            hdr[1] = hdr[2] = hdr[0];
         }
         else
         {
            if (r && g)
            {
               gamma[1] = gamma[0];
               brightness[1] = brightness[0];
               hdr[1] = hdr[0];
            }
            else if (r & b)
            {
               gamma[2] = gamma[0];
               brightness[2] = brightness[0];
               hdr[2] = hdr[0];
            }
            else if (g & b)
            {
               gamma[2] = gamma[1];
               brightness[2] = brightness[1];
               hdr[2] = hdr[1];
            }
         }
         m_glview->SetGamma(gamma);
         m_glview->SetBrightness(brightness);
         m_glview->SetHdr(hdr);
         m_glview->SetSyncR(r);
         m_glview->SetSyncG(g);
         m_glview->SetSyncB(b);

         //sync properties of the selcted volume
         VolumeData* svd = vr_frame->GetCurSelVol();
         if (!svd)
            svd = GetAllVolumeData(0);
         if (svd)
         {
            for (int i=0; i<GetAllVolumeNum(); i++)
            {
               VolumeData* vd = GetAllVolumeData(i);
               if (vd)
               {
                  vd->SetNR(svd->GetNR());
                  vd->SetSampleRate(svd->GetSampleRate());
                  vd->SetShadow(svd->GetShadow());
                  double sp;
                  svd->GetShadowParams(sp);
                  vd->SetShadowParams(sp);
               }
            }
         }
      }
      else if (mode_switch_type == 2)
      {
         if (GetGroupNum() == 1)
         {
            for (int i=0; i<GetLayerNum(); i++)
            {
               TreeLayer* layer = GetLayer(i);
               if (layer && layer->IsA() == 5)
               {
                  DataGroup* group = (DataGroup*)layer;
                  FLIVR::Color col = m_glview->GetGamma();
                  group->SetGammaAll(col);
                  col = m_glview->GetBrightness();
                  group->SetBrightnessAll(col);
                  col = m_glview->GetHdr();
                  group->SetHdrAll(col);
                  group->SetSyncRAll(m_glview->GetSyncR());
                  group->SetSyncGAll(m_glview->GetSyncG());
                  group->SetSyncBAll(m_glview->GetSyncB());
                  break;
               }
            }
         }
      }

      vr_frame->GetTree()->UpdateSelection();
   }

   RefreshGL();
}

//ch1
void VRenderView::OnCh1Check(wxCommandEvent &event)
{
   wxCheckBox* ch1 = (wxCheckBox*)event.GetEventObject();
   if (ch1)
      VRenderFrame::SetCompression(ch1->GetValue());
}

//embde project
void VRenderView::OnChEmbedCheck(wxCommandEvent &event)
{
   wxCheckBox* ch_embed = (wxCheckBox*)event.GetEventObject();
   if (ch_embed)
      VRenderFrame::SetEmbedProject(ch_embed->GetValue());
}

wxWindow* VRenderView::CreateExtraCaptureControl(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, 0, wxDefaultPosition, wxSize(400, 90));

   wxBoxSizer *group1 = new wxStaticBoxSizer(
         new wxStaticBox(panel, wxID_ANY, "Additional Options"), wxVERTICAL);

   //compressed
   wxCheckBox* ch1 = new wxCheckBox(panel, wxID_HIGHEST+3004,
         "Lempel-Ziv-Welch Compression");
   ch1->Connect(ch1->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
         wxCommandEventHandler(VRenderView::OnCh1Check), NULL, panel);
   if (ch1)
      ch1->SetValue(VRenderFrame::GetCompression());

   //copy all files check box
   wxCheckBox* ch_embed = 0;
   if (VRenderFrame::GetSaveProject())
   {
      ch_embed = new wxCheckBox(panel, wxID_HIGHEST+3005,
            "Embed all files in the project folder");
      ch_embed->Connect(ch_embed->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
            wxCommandEventHandler(VRenderView::OnChEmbedCheck), NULL, panel);
      ch_embed->SetValue(VRenderFrame::GetEmbedProject());
   }

   //group
   group1->Add(10, 10);
   group1->Add(ch1);
   group1->Add(10, 10);
   if (VRenderFrame::GetSaveProject() &&
         ch_embed)
   {
      group1->Add(ch_embed);
      group1->Add(10, 10);
   }

   panel->SetSizer(group1);
   panel->Layout();

   return panel;
}

void VRenderView::OnCapture(wxCommandEvent& event)
{
   VRenderFrame* vr_frame = (VRenderFrame*)m_frame;

   if (vr_frame && vr_frame->GetSettingDlg())
      VRenderFrame::SetSaveProject(vr_frame->GetSettingDlg()->GetProjSave());

   wxFileDialog file_dlg(this, "Save captured image", "", "", "*.tif", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
   file_dlg.SetExtraControlCreator(CreateExtraCaptureControl);
   int rval = file_dlg.ShowModal();
   if (rval == wxID_OK)
   {
      m_glview->m_cap_file = file_dlg.GetDirectory() + "/" + file_dlg.GetFilename();
      m_glview->m_capture = true;
      RefreshGL();

      if (vr_frame && vr_frame->GetSettingDlg() &&
            vr_frame->GetSettingDlg()->GetProjSave())
      {
         wxString new_folder;
         new_folder = m_glview->m_cap_file + "_project";
         CREATE_DIR(new_folder.fn_str());
         wxString prop_file = new_folder + "/" + file_dlg.GetFilename() + "_project.vrp";
         vr_frame->SaveProject(prop_file);
      }
   }
}


//bar left
void VRenderView::OnDepthAttenCheck(wxCommandEvent& event)
{
   if (m_depth_atten_chk->GetValue())
   {
      SetFog(true);
      m_depth_atten_factor_sldr->Enable();
      m_depth_atten_factor_text->Enable();
   }
   else
   {
      SetFog(false);
      m_depth_atten_factor_sldr->Disable();
      m_depth_atten_factor_text->Disable();
   }

   RefreshGL();
}

void VRenderView::OnDepthAttenFactorChange(wxScrollEvent& event)
{
   double atten_factor = m_depth_atten_factor_sldr->GetValue()/100.0;
   wxString str = wxString::Format("%.2f", atten_factor);
   m_depth_atten_factor_text->SetValue(str);
}

void VRenderView::OnDepthAttenFactorEdit(wxCommandEvent& event)
{
   wxString str = m_depth_atten_factor_text->GetValue();
   double val;
   str.ToDouble(&val);
   SetFogIntensity(val);
   m_depth_atten_factor_sldr->SetValue(int(val*100.0));
   RefreshGL(true);
}

void VRenderView::OnDepthAttenReset(wxCommandEvent &event)
{
   if (m_use_dft_settings)
   {
      wxString str = wxString::Format("%.2f", m_dft_depth_atten_factor);
      m_depth_atten_factor_text->SetValue(str);
   }
   else
   {
      wxString str = wxString::Format("%.2f", 0.0);
      m_depth_atten_factor_text->SetValue(str);
   }
}

//bar right
void VRenderView::OnCenter(wxCommandEvent &event)
{
   if (m_glview)
   {
      m_glview->SetCenter();
      RefreshGL();
   }
}

void VRenderView::OnScale121(wxCommandEvent &event)
{
   if (m_glview)
   {
      m_glview->SetScale121();
      RefreshGL();
      if (m_glview->m_mouse_focus)
         m_glview->SetFocus();
   }
}

void VRenderView::OnScaleFactorChange(wxScrollEvent& event)
{
   int scale_factor = m_scale_factor_sldr->GetValue();
   m_glview->m_scale_factor = scale_factor/100.0;
   wxString str = wxString::Format("%d", scale_factor);
   m_scale_factor_text->SetValue(str);
}

void VRenderView::OnScaleFactorEdit(wxCommandEvent& event)
{
   wxString str = m_scale_factor_text->GetValue();
   long val;
   str.ToLong(&val);
   if (val>0)
   {
      m_scale_factor_sldr->SetValue(val);
      m_glview->m_scale_factor = val/100.0;
      //m_glview->SetSortBricks();
      RefreshGL(true);
   }
}

void VRenderView::OnScaleFactorSpinUp(wxSpinEvent& event)
{
   wxString str_val = m_scale_factor_text->GetValue();
   long val;
   str_val.ToLong(&val);
   val++;
   str_val = wxString::Format("%d", val);
   m_scale_factor_text->SetValue(str_val);
}

void VRenderView::OnScaleFactorSpinDown(wxSpinEvent& event)
{
   wxString str_val = m_scale_factor_text->GetValue();
   long val;
   str_val.ToLong(&val);
   val--;
   str_val = wxString::Format("%d", val);
   m_scale_factor_text->SetValue(str_val);
}

void VRenderView::OnScaleReset(wxCommandEvent &event)
{
   if (m_use_dft_settings)
   {
      wxString str = wxString::Format("%d", int(m_dft_scale_factor));
      m_scale_factor_text->SetValue(str);
   }
   else
      m_scale_factor_text->SetValue("100");
   if (m_glview && m_glview->m_mouse_focus)
      m_glview->SetFocus();
}

//bar bottom
void VRenderView::UpdateView(bool ui_update)
{
   double rotx, roty, rotz;
   wxString str_val = m_x_rot_text->GetValue();
   rotx = STOD(str_val.fn_str());
   str_val = m_y_rot_text->GetValue();
   roty = STOD(str_val.fn_str());
   str_val = m_z_rot_text->GetValue();
   rotz = STOD(str_val.fn_str());
   SetRotations(rotx, roty, rotz, ui_update);
   RefreshGL(true);
}

void VRenderView::OnValueEdit(wxCommandEvent& event)
{
   UpdateView(false);
}

void VRenderView::OnRotLink(wxCommandEvent& event)
{
   if (m_rot_link_chk->GetValue())
   {
      m_glview->m_linked_rot = true;
      double rotx, roty, rotz;
      m_glview->GetRotations(rotx, roty, rotz);
      VRenderFrame* vr_frame = (VRenderFrame*)m_frame;
      if (vr_frame)
      {
         for (int i=0; i<vr_frame->GetViewNum(); i++)
         {
            VRenderView* view = vr_frame->GetView(i);
            if (view)
            {
               view->m_glview->m_linked_rot = true;
               view->m_rot_link_chk->SetValue(true);
               view->m_glview->SetRotations(rotx, roty, rotz);
            }
         }
      }
   }
   else
      m_glview->m_linked_rot = false;
}

void VRenderView::OnRotReset(wxCommandEvent &event)
{
   m_x_rot_sldr->SetValue(0);
   m_y_rot_sldr->SetValue(0);
   m_z_rot_sldr->SetValue(0);
   m_x_rot_text->ChangeValue("0.0");
   m_y_rot_text->ChangeValue("0.0");
   m_z_rot_text->ChangeValue("0.0");
   SetRotations(0.0, 0.0, 0.0);
   RefreshGL();
   if (m_glview->m_mouse_focus)
      m_glview->SetFocus();
}

void VRenderView::OnXRotScroll(wxScrollEvent& event)
{
   int val = event.GetPosition();
   double dval = m_glview->GetRotLock()?val*45.0:val;
   wxString str = wxString::Format("%.1f", dval);
   m_x_rot_text->SetValue(str);
}

void VRenderView::OnYRotScroll(wxScrollEvent& event)
{
   int val = event.GetPosition();
   double dval = m_glview->GetRotLock()?val*45.0:val;
   wxString str = wxString::Format("%.1f", dval);
   m_y_rot_text->SetValue(str);
}

void VRenderView::OnZRotScroll(wxScrollEvent& event)
{
   int val = event.GetPosition();
   double dval = m_glview->GetRotLock()?val*45.0:val;
   wxString str = wxString::Format("%.1f", dval);
   m_z_rot_text->SetValue(str);
}

void VRenderView::OnRotLockCheck(wxCommandEvent& event)
{
   if (m_rot_lock_chk->GetValue())
   {
      m_glview->SetRotLock(true);
      //sliders
      m_x_rot_sldr->SetRange(0, 8);
      m_y_rot_sldr->SetRange(0, 8);
      m_z_rot_sldr->SetRange(0, 8);
      double rotx, roty, rotz;
      m_glview->GetRotations(rotx, roty, rotz);
      m_x_rot_sldr->SetValue(int(rotx/45.0));
      m_y_rot_sldr->SetValue(int(roty/45.0));
      m_z_rot_sldr->SetValue(int(rotz/45.0));
   }
   else
   {
      m_glview->SetRotLock(false);
      //sliders
      m_x_rot_sldr->SetRange(0, 360);
      m_y_rot_sldr->SetRange(0, 360);
      m_z_rot_sldr->SetRange(0, 360);
      double rotx, roty, rotz;
      m_glview->GetRotations(rotx, roty, rotz);
      m_x_rot_sldr->SetValue(int(rotx));
      m_y_rot_sldr->SetValue(int(roty));
      m_z_rot_sldr->SetValue(int(rotz));
   }
}

void VRenderView::OnXRotSpinUp(wxSpinEvent& event)
{
   wxString str_val = m_x_rot_text->GetValue();
   int value = STOI(str_val.fn_str())+1;
   value = value>360?value-360:value;
   wxString str = wxString::Format("%.1f", double(value));
   m_x_rot_text->SetValue(str);
}

void VRenderView::OnXRotSpinDown(wxSpinEvent& event)
{
   wxString str_val = m_x_rot_text->GetValue();
   int value = STOI(str_val.fn_str())-1;
   value = value<0?value+360:value;
   wxString str = wxString::Format("%.1f", double(value));
   m_x_rot_text->SetValue(str);
}

void VRenderView::OnYRotSpinUp(wxSpinEvent& event)
{
   wxString str_val = m_y_rot_text->GetValue();
   int value = STOI(str_val.fn_str())+1;
   value = value>360?value-360:value;
   wxString str = wxString::Format("%.1f", double(value));
   m_y_rot_text->SetValue(str);
}

void VRenderView::OnYRotSpinDown(wxSpinEvent& event)
{
   wxString str_val = m_y_rot_text->GetValue();
   int value = STOI(str_val.fn_str())-1;
   value = value<0?value+360:value;
   wxString str = wxString::Format("%.1f", double(value));
   m_y_rot_text->SetValue(str);
}

void VRenderView::OnZRotSpinUp(wxSpinEvent& event)
{
   wxString str_val = m_z_rot_text->GetValue();
   int value = STOI(str_val.fn_str())+1;
   value = value>360?value-360:value;
   wxString str = wxString::Format("%.1f", double(value));
   m_z_rot_text->SetValue(str);
}

void VRenderView::OnZRotSpinDown(wxSpinEvent& event)
{
   wxString str_val = m_z_rot_text->GetValue();
   int value = STOI(str_val.fn_str())-1;
   value = value<0?value+360:value;
   wxString str = wxString::Format("%.1f", double(value));
   m_z_rot_text->SetValue(str);
}

//top
void VRenderView::OnBgColorChange(wxColourPickerEvent& event)
{
   wxColor c = event.GetColour();
   Color color(c.Red()/255.0, c.Green()/255.0, c.Blue()/255.0);
   SetBackgroundColor(color);
   RefreshGL();
}

void VRenderView::OnCamCtrCheck(wxCommandEvent& event)
{
   m_glview->m_draw_camctr = m_cam_ctr_chk->GetValue();
   RefreshGL();
}

void VRenderView::OnFpsCheck(wxCommandEvent& event)
{
   m_glview->m_draw_info = m_fps_chk->GetValue();
   m_glview->m_draw_coord = m_glview->m_draw_info;
   RefreshGL();
}

void VRenderView::OnLegendCheck(wxCommandEvent& event)
{
   m_glview->m_draw_legend = m_legend_chk->GetValue();
   RefreshGL();
}

void VRenderView::OnIntpCheck(wxCommandEvent& event)
{
   m_glview->SetIntp(m_intp_chk->GetValue());
   RefreshGL();
}

void VRenderView::OnSearchCheck(wxCommandEvent& event)
{
	m_glview->SetSearcherVisibility(m_search_chk->GetValue());
}

void VRenderView::OnAovSldrIdle(wxIdleEvent& event)
{
   if (m_glview->m_capture)
      return;

   wxPoint pos = wxGetMousePosition();
   wxRect reg = m_aov_sldr->GetScreenRect();
   if (reg.Contains(pos))
   {
      if (!m_draw_clip)
      {
         m_glview->m_draw_clip = true;
         m_glview->m_clip_mask = -1;
         RefreshGLOverlays();
         m_draw_clip = true;
      }
   }
   else
   {
      if (m_draw_clip)
      {
         m_glview->m_draw_clip = false;
         RefreshGLOverlays();
         m_draw_clip = false;
      }
   }
   event.Skip();
}

void VRenderView::OnAovChange(wxScrollEvent& event)
{
   int val = m_aov_sldr->GetValue();
   m_aov_text->SetValue(wxString::Format("%d", val));
}

void VRenderView::OnAovText(wxCommandEvent& event)
{
   wxString str = m_aov_text->GetValue();
   long val;
   str.ToLong(&val);
   if (val == 0 || val == 10)
   {
      SetPersp(false);
      m_aov_text->ChangeValue("Ortho");
      m_aov_sldr->SetValue(10);
   }
   else if (val <= 10)
   {
      return;
   }
   else
   {
      if (val > 100)
      {
         val = 100;
         m_aov_text->ChangeValue("100");
         m_aov_sldr->SetValue(100);
      }
      SetPersp(true);
      SetAov(val);
      m_aov_sldr->SetValue(val);
   }
   RefreshGL(true);
}

void VRenderView::OnFreeChk(wxCommandEvent& event)
{
   if (m_free_chk->GetValue())
      SetFree(true);
   else
   {
      SetFree(false);
      int val = m_aov_sldr->GetValue();
      if (val == 10)
         SetPersp(false);
      else
         SetPersp(true);
   }
   RefreshGL();
}

void VRenderView::OnPPIChange(wxScrollEvent& event)
{
	int val = m_ppi_sldr->GetValue();
	m_ppi_text->SetValue(wxString::Format("%d", val));
}

void VRenderView::OnPPIEdit(wxCommandEvent &event)
{
	wxString str = m_ppi_text->GetValue();
	long val;
	str.ToLong(&val);
	if (val>0)
	{
		m_ppi_sldr->SetValue(val);
		m_glview->m_min_ppi = val;
		RefreshGL(true);
	}
}

void VRenderView::OnSaveDefault(wxCommandEvent &event)
{
   wxFileConfig fconfig("FluoRender default view settings");
   wxString str;

   //render modes
   bool bVal;
   bVal = m_volume_seq_rd->GetValue();
   fconfig.Write("volume_seq_rd", bVal);
   bVal = m_volume_multi_rd->GetValue();
   fconfig.Write("volume_multi_rd", bVal);
   bVal = m_volume_comp_rd->GetValue();
   fconfig.Write("volume_comp_rd", bVal);
   //background color
   wxColor cVal;
   cVal = m_bg_color_picker->GetColour();
   str = wxString::Format("%d %d %d", cVal.Red(), cVal.Green(), cVal.Blue());
   fconfig.Write("bg_color_picker", str);
   //camera center
   bVal = m_cam_ctr_chk->GetValue();
   fconfig.Write("cam_ctr_chk", bVal);
   //camctr size
   fconfig.Write("camctr_size", m_glview->m_camctr_size);
   //fps
   bVal = m_fps_chk->GetValue();
   fconfig.Write("fps_chk", bVal);
   //interpolation
   bVal = m_intp_chk->GetValue();
   fconfig.Write("intp_chk", bVal);
   //selection
   bVal = m_glview->m_draw_legend;
   fconfig.Write("legend_chk", bVal);
   //mouse focus
   bVal = m_glview->m_mouse_focus;
   fconfig.Write("mouse_focus", bVal);
   //search
   bVal = m_search_chk->GetValue();
   fconfig.Write("search_chk", bVal);
   //ortho/persp
   fconfig.Write("persp", m_glview->m_persp);
   fconfig.Write("aov", m_glview->m_aov);
   bVal = m_free_chk->GetValue();
   fconfig.Write("free_rd", bVal);
   //min dpi
   fconfig.Write("min_dpi", m_glview->m_min_ppi);
   //rotations
   str = m_x_rot_text->GetValue();
   fconfig.Write("x_rot", str);
   str = m_y_rot_text->GetValue();
   fconfig.Write("y_rot", str);
   str = m_z_rot_text->GetValue();
   fconfig.Write("z_rot", str);
   fconfig.Write("rot_lock", m_glview->GetRotLock());
   //depth atten
   bVal = m_depth_atten_chk->GetValue();
   fconfig.Write("depth_atten_chk", bVal);
   str = m_depth_atten_factor_text->GetValue();
   fconfig.Write("depth_atten_factor_text", str);
   str.ToDouble(&m_dft_depth_atten_factor);
   //scale factor
   str = m_scale_factor_text->GetValue();
   fconfig.Write("scale_factor_text", str);
   str.ToDouble(&m_dft_scale_factor);
   //camera center
   double x, y, z;
   GetCenters(x, y, z);
   str = wxString::Format("%f %f %f", x, y, z);
   fconfig.Write("center", str);
#ifdef _DARWIN
    wxString dft = wxString(getenv("HOME")) + "/Fluorender.settings/";
    mkdir(dft,0777);
    chmod(dft,0777);
    dft = dft + "default_view_settings.dft";
#else
    wxString dft = wxStandardPaths::Get().GetLocalDataDir() + wxFileName::GetPathSeparator() + "default_view_settings.dft";
#endif
   wxFileOutputStream os(dft);
   fconfig.Save(os);
}

void VRenderView::LoadSettings()
{
#ifdef _DARWIN
    
    wxString dft = wxString(getenv("HOME")) + "/Fluorender.settings/default_view_settings.dft";
    std::ifstream tmp(dft);
    if (!tmp.good())
        dft = "FluoRender.app/Contents/Resources/default_view_settings.dft";
    else
        tmp.close();
#else
	wxString dft = wxStandardPaths::Get().GetLocalDataDir() + wxFileName::GetPathSeparator() + "default_view_settings.dft";
#endif
    
    wxFileInputStream is(dft);
    
    if (!is.IsOk()) {
        UpdateView();
        return;
    }
    
   wxFileConfig fconfig(is);

   bool bVal;
   double dVal;
   if (fconfig.Read("volume_seq_rd", &bVal))
   {
      m_volume_seq_rd->SetValue(bVal);
      if (bVal)
         SetVolMethod(VOL_METHOD_SEQ);
   }
   if (fconfig.Read("volume_multi_rd", &bVal))
   {
      m_volume_multi_rd->SetValue(bVal);
      if (bVal)
         SetVolMethod(VOL_METHOD_MULTI);
   }
   if (fconfig.Read("volume_comp_rd", &bVal))
   {
      m_volume_comp_rd->SetValue(bVal);
      if (bVal)
         SetVolMethod(VOL_METHOD_COMP);
   }
   wxString str;
   if (fconfig.Read("bg_color_picker", &str))
   {
      int r, g, b;
      SSCANF(str, "%d%d%d", &r, &g, &b);
      wxColor cVal(r, g, b);
      m_bg_color_picker->SetColour(cVal);
      Color c(r/255.0, g/255.0, b/255.0);
      SetBackgroundColor(c);
   }
   if (fconfig.Read("cam_ctr_chk", &bVal))
   {
      m_cam_ctr_chk->SetValue(bVal);
      m_glview->m_draw_camctr = bVal;
   }
   if (fconfig.Read("camctr_size", &dVal))
   {
      m_glview->m_camctr_size = dVal;
   }
   if (fconfig.Read("fps_chk", &bVal))
   {
      m_fps_chk->SetValue(bVal);
      m_glview->m_draw_info = bVal;
      m_glview->m_draw_coord = bVal;
   }
   if (fconfig.Read("intp_chk", &bVal))
   {
      m_intp_chk->SetValue(bVal);
      m_glview->SetIntp(bVal);
   }
   if (fconfig.Read("legend_chk", &bVal))
   {
      m_legend_chk->SetValue(bVal);
      m_glview->m_draw_legend = bVal;
   }
   if (fconfig.Read("search_chk", &bVal))
   {
      m_search_chk->SetValue(bVal);
	  if (bVal) m_glview->m_searcher->Show();
	  else m_glview->m_searcher->Hide();
   }
    if (fconfig.Read("mouse_focus", &bVal))
   {
      m_glview->m_mouse_focus = bVal;
   }
   if (fconfig.Read("persp", &bVal))
   {
      if (bVal)
         SetPersp(true);
      else
         SetPersp(false);
   }
   if (fconfig.Read("aov", &dVal))
   {
      SetAov(dVal);
   }
   if (fconfig.Read("min_dpi", &dVal))
   {
	   SetMinPPI(dVal);
   }
   if (fconfig.Read("free_rd", &bVal))
   {
      m_free_chk->SetValue(bVal);
      if (bVal)
         SetFree(true);
   }
   if (fconfig.Read("x_rot", &str))
   {
      m_x_rot_text->ChangeValue(str);
      str.ToDouble(&m_dft_x_rot);
   }
   if (fconfig.Read("y_rot", &str))
   {
      m_y_rot_text->ChangeValue(str);
      str.ToDouble(&m_dft_y_rot);
   }
   if (fconfig.Read("z_rot", &str))
   {
      m_z_rot_text->ChangeValue(str);
      str.ToDouble(&m_dft_z_rot);
   }
   if (fconfig.Read("rot_lock", &bVal))
   {
      m_rot_lock_chk->SetValue(bVal);
      m_glview->SetRotLock(bVal);
   }
   UpdateView();  //for rotations
   if (fconfig.Read("scale_factor_text", &str))
   {
      m_scale_factor_text->ChangeValue(str);
      str.ToDouble(&dVal);
      m_scale_factor_sldr->SetValue(dVal);
      m_glview->m_scale_factor = dVal/100.0;
      m_dft_scale_factor = dVal;
   }
   if (fconfig.Read("depth_atten_chk", &bVal))
   {
      m_depth_atten_chk->SetValue(bVal);
      if (bVal)
         SetFog(true);
      else
         SetFog(false);
   }
   if (fconfig.Read("depth_atten_factor_text", &str))
   {
      m_depth_atten_factor_text->ChangeValue(str);
      str.ToDouble(&dVal);
      m_depth_atten_factor_sldr->SetValue(int(dVal*100));
      SetFogIntensity(dVal);
      m_dft_depth_atten_factor = dVal;
   }
   if (fconfig.Read("center", &str))
   {
      float x, y, z;
      SSCANF(str, "%f%f%f", &x, &y, &z);
      SetCenters(x, y, z);
   }

   m_use_dft_settings = true;
   RefreshGL();
}

void VRenderView::OnKeyDown(wxKeyEvent& event)
{
   event.Skip();
}

