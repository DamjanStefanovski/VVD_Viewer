/*
For more information, please see: http://software.sci.utah.edu

The MIT License

Copyright (c) 2014 Scientific Computing and Imaging Institute,
University of Utah.


Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
#include "VRenderFrame.h"
#include "DragDrop.h"
#include <wx/artprov.h>
#include <wx/wfstream.h>
#include <wx/fileconf.h>
#include <wx/aboutdlg.h>
#include <wx/progdlg.h>
#include <wx/hyperlink.h>
#include <wx/stdpaths.h>
#include "Formats/png_resource.h"
#include "Formats/msk_writer.h"
#include "Formats/msk_reader.h"
#include "Converters/VolumeMeshConv.h"
#include "compatibility.h"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <curl/curl.h>

//resources
#include "img/icons.h"

BEGIN_EVENT_TABLE(VRenderFrame, wxFrame)
	EVT_MENU(wxID_EXIT, VRenderFrame::OnExit)
	EVT_MENU(ID_ViewNew, VRenderFrame::OnNewView)
	EVT_MENU(ID_FullScreen, VRenderFrame::OnFullScreen)
	EVT_MENU(ID_OpenVolume, VRenderFrame::OnOpenVolume)
	EVT_MENU(ID_DownloadVolume, VRenderFrame::OnDownloadVolume)
	EVT_MENU(ID_OpenMesh, VRenderFrame::OnOpenMesh)
	EVT_MENU(ID_ViewOrganize, VRenderFrame::OnOrganize)
	EVT_MENU(ID_CheckUpdates, VRenderFrame::OnCheckUpdates)
	EVT_MENU(ID_Info, VRenderFrame::OnInfo)
	EVT_MENU(ID_CreateCube, VRenderFrame::OnCreateCube)
	EVT_MENU(ID_CreateSphere, VRenderFrame::OnCreateSphere)
	EVT_MENU(ID_CreateCone, VRenderFrame::OnCreateCone)
	EVT_MENU(ID_SaveProject, VRenderFrame::OnSaveProject)
	EVT_MENU(ID_OpenProject, VRenderFrame::OnOpenProject)
	EVT_MENU(ID_Settings, VRenderFrame::OnSettings)
	EVT_MENU(ID_PaintTool, VRenderFrame::OnPaintTool)
	EVT_MENU(ID_NoiseCancelling, VRenderFrame::OnNoiseCancelling)
	EVT_MENU(ID_Counting, VRenderFrame::OnCounting)
	EVT_MENU(ID_Colocalization, VRenderFrame::OnColocalization)
	EVT_MENU(ID_Convert, VRenderFrame::OnConvert)
	EVT_MENU(ID_Recorder, VRenderFrame::OnRecorder)
	EVT_MENU(ID_InfoDlg, VRenderFrame::OnInfoDlg)
	EVT_MENU(ID_Trace, VRenderFrame::OnTrace)
	EVT_MENU(ID_Measure, VRenderFrame::OnMeasure)
	EVT_MENU(ID_Twitter, VRenderFrame::OnTwitter)
	EVT_MENU(ID_Facebook, VRenderFrame::OnFacebook)
	EVT_MENU(ID_ShowHideUI, VRenderFrame::OnShowHideUI)
	EVT_MENU(ID_ShowHideToolbar, VRenderFrame::OnShowHideToolbar)
	//ui menu events
	EVT_MENU(ID_UITreeView, VRenderFrame::OnShowHideView)
	EVT_MENU(ID_UIMeasureView, VRenderFrame::OnShowHideView)
	EVT_MENU(ID_UIMovieView, VRenderFrame::OnShowHideView)
	EVT_MENU(ID_UIAdjView, VRenderFrame::OnShowHideView)
	EVT_MENU(ID_UIClipView, VRenderFrame::OnShowHideView)
	EVT_MENU(ID_UIPropView, VRenderFrame::OnShowHideView)
	//panes
	EVT_AUI_PANE_CLOSE(VRenderFrame::OnPaneClose)
	//draw background
	EVT_PAINT(VRenderFrame::OnDraw)
	//process key event
	EVT_KEY_DOWN(VRenderFrame::OnKeyDown)
	//close
	EVT_CLOSE(VRenderFrame::OnClose)
END_EVENT_TABLE()

bool VRenderFrame::m_sliceSequence = false;
bool VRenderFrame::m_timeSequence = false;
bool VRenderFrame::m_compression = false;
bool VRenderFrame::m_skip_brick = false;
wxString VRenderFrame::m_time_id = "_T";
bool VRenderFrame::m_load_mask = true;
bool VRenderFrame::m_save_compress = true;
bool VRenderFrame::m_vrp_embed = false;
bool VRenderFrame::m_save_project = false;

CURLM *_g_curlm;//add by takashi
CURL *_g_curl;//add by takashi

VRenderFrame::VRenderFrame(
	wxFrame* frame,
	const wxString& title,
	int x, int y,
	int w, int h)
	: wxFrame(frame, wxID_ANY, title, wxPoint(x, y), wxSize(w, h),wxDEFAULT_FRAME_STYLE),
	m_mov_view(0),
	m_movie_view(0),
	m_anno_view(0),
	m_tree_panel(0),
	m_prop_panel(0),
	m_setting_dlg(0),
	m_ui_state(true),
	m_cur_sel_type(-1),
	m_cur_sel_vol(-1),
	m_cur_sel_mesh(-1),
	m_gpu_max_mem(-1.0)
{
	SetEvtHandlerEnabled(false);
	Freeze();

	curl_global_init(CURL_GLOBAL_ALL);//add by takashi
	_g_curlm = curl_multi_init();//add by takashi
	_g_curl = curl_easy_init();//add by takashi
    
    FLIVR::TextureBrick::setCURL(_g_curl);
	FLIVR::TextureBrick::setCURL_Multi(_g_curlm);

	//create this first to read the settings
	m_setting_dlg = new SettingDlg(this, this);

	// tell wxAuiManager to manage this frame
	m_aui_mgr.SetManagedWindow(this);

	// set frame icon
	wxIcon icon;
	icon.CopyFromBitmap(wxGetBitmapFromMemory(icon_32));
	SetIcon(icon);

	// create the main toolbar
	m_main_tb = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxTB_FLAT|wxTB_TOP|wxTB_NODIVIDER);
	//create the menu for UI management
	m_tb_menu_ui = new wxMenu;
	m_tb_menu_ui->Append(ID_UITreeView, UITEXT_TREEVIEW,
		"Show/hide the workspace panel", wxITEM_CHECK);
	m_tb_menu_ui->Append(ID_UIMeasureView, UITEXT_MEASUREMENT,
		"Show/hide the measurement panel", wxITEM_CHECK);
	m_tb_menu_ui->Append(ID_UIMovieView, UITEXT_MAKEMOVIE,
		"Show/hide the movie panel", wxITEM_CHECK);
	m_tb_menu_ui->Append(ID_UIAdjView, UITEXT_ADJUST,
		"Show/hide the output adjustment panel", wxITEM_CHECK);
	m_tb_menu_ui->Append(ID_UIClipView, UITEXT_CLIPPING,
		"Show/hide the clipping plane control panel", wxITEM_CHECK);
	m_tb_menu_ui->Append(ID_UIPropView, UITEXT_PROPERTIES,
		"Show/hide the property panel", wxITEM_CHECK);
	//check all the items
	m_tb_menu_ui->Check(ID_UITreeView, true);
	m_tb_menu_ui->Check(ID_UIMeasureView, false);
	m_tb_menu_ui->Check(ID_UIMovieView, true);
	m_tb_menu_ui->Check(ID_UIAdjView, true);
	m_tb_menu_ui->Check(ID_UIClipView, true);
	m_tb_menu_ui->Check(ID_UIPropView, true);
	//create the menu for edit/convert
	m_tb_menu_edit = new wxMenu;
	m_tb_menu_edit->Append(ID_PaintTool, "Analyze...",
		"Show analysis tools for volume data");
	m_tb_menu_edit->Append(ID_Measure, "Measurement...",
		"Show measurement tools");
	m_tb_menu_edit->Append(ID_Trace, "Components && Tracking...",
		"Show tracking tools");
	m_tb_menu_edit->Append(ID_NoiseCancelling, "Noise Reduction...",
		"Show noise reduction dialog");
	m_tb_menu_edit->Append(ID_Counting, "Counting and Volume...",
		"Show voxel counting and volume calculation tools");
	m_tb_menu_edit->Append(ID_Colocalization, "Colocalization Analysis...",
		"Show colocalization analysis tools");
	m_tb_menu_edit->Append(ID_Convert, "Convert...",
		"Show tools for volume to mesh conversion");
	m_tb_menu_edit->Append(ID_InfoDlg, "Infomation...",
		"Display file information");
	//build the main toolbar
	//add tools
	m_main_tb->AddTool(ID_OpenVolume, "Open Volume",
		wxGetBitmapFromMemory(icon_open_volume), wxNullBitmap, wxITEM_NORMAL,
		"Open single or multiple volume data file(s)",
		"Open single or multiple volume data file(s)");
#ifdef WITH_DATABASE
	m_main_tb->AddTool(ID_DownloadVolume, "Database",
		wxGetBitmapFromMemory(icon_database), wxNullBitmap, wxITEM_NORMAL,
		"Database: Download single or multiple volume data file(s)",
		"Database: Download single or multiple volume data file(s)");
#endif // WITH_DATABASE
	m_main_tb->AddTool(ID_OpenProject, "Open Project",
		wxGetBitmapFromMemory(icon_open_project), wxNullBitmap, wxITEM_NORMAL,
		"Open a saved project",
		"Open a saved project");
	m_main_tb->AddTool(ID_SaveProject, "Save Project",
		wxGetBitmapFromMemory(icon_save_project), wxNullBitmap, wxITEM_NORMAL,
		"Save current work as a project",
		"Save current work as a project");
	m_main_tb->AddSeparator();
	m_main_tb->AddTool(ID_ViewNew, "New View",
		wxGetBitmapFromMemory(icon_new_view), wxNullBitmap, wxITEM_NORMAL,
		"Create a new render viewport",
		"Create a new render viewport");
	m_main_tb->AddTool(ID_ShowHideUI, "Show/Hide UI",
		wxGetBitmapFromMemory(icon_show_hide_ui), wxNullBitmap, wxITEM_DROPDOWN,
		"Show or hide all control panels",
		"Show or hide all control panels");
	m_main_tb->SetDropdownMenu(ID_ShowHideUI, m_tb_menu_ui);
	m_main_tb->AddSeparator();
	m_main_tb->AddTool(ID_OpenMesh, "Open Mesh",
		wxGetBitmapFromMemory(icon_open_mesh), wxNullBitmap, wxITEM_NORMAL,
		"Open single or multiple mesh file(s)",
		"Open single or multiple mesh file(s)");
    m_main_tb->AddTool(ID_PaintTool, "Analyze",
                       wxGetBitmapFromMemory(icon_edit), wxNullBitmap,
                       wxITEM_DROPDOWN,
                       "Tools for analyzing selected channel",
                       "Tools for analyzing selected channel");
    m_main_tb->SetDropdownMenu(ID_PaintTool, m_tb_menu_edit);
	m_main_tb->AddTool(ID_Recorder, "Recorder",
		wxGetBitmapFromMemory(icon_recorder), wxNullBitmap, wxITEM_NORMAL,
		"Recorder: Record actions by key frames and play back",
		"Recorder: Record actions by key frames and play back");
	m_main_tb->AddSeparator();
	m_main_tb->AddTool(ID_Settings, "Settings",
		wxGetBitmapFromMemory(icon_settings), wxNullBitmap, wxITEM_NORMAL,
		"Settings of FluoRender",
		"Settings of FluoRender");
	m_main_tb->AddStretchableSpace();
/*	m_main_tb->AddTool(ID_CheckUpdates, "Update",
		wxGetBitmapFromMemory(icon_check_updates), wxNullBitmap, wxITEM_NORMAL,
		"Check if there is a new release",
		"Check if there is a new release (requires Internet connection)");
	m_main_tb->AddTool(ID_Facebook, "Facebook",
		wxGetBitmapFromMemory(icon_facebook), wxNullBitmap, wxITEM_NORMAL,
		"FluoRender's facebook page",
		"FluoRender's facebook page (requires Internet connection)");
	m_main_tb->AddTool(ID_Twitter, "Twitter",
		wxGetBitmapFromMemory(icon_twitter), wxNullBitmap, wxITEM_NORMAL,
		"Follow FluoRender on Twitter",
		"Follow FluoRender on Twitter (requires Internet connection)");
	m_main_tb->AddTool(ID_Info, "About",
		wxGetBitmapFromMemory(icon_about), wxNullBitmap, wxITEM_NORMAL,
		"FluoRender information",
		"FluoRender information");
*/
	m_main_tb->Realize();

	//create render view
	VRenderView *vrv = new VRenderView(this, this, wxID_ANY);
	vrv->SetDropTarget(new DnDFile(this, vrv));
	vrv->InitView();
	m_vrv_list.push_back(vrv);

	//create tree view
	m_tree_panel = new TreePanel(this, this, wxID_ANY,
		wxDefaultPosition, wxSize(320, 300));

	//create movie view (sets the m_recorder_dlg)
	m_movie_view = new VMovieView(this, this, wxID_ANY,
		wxDefaultPosition, wxSize(320, 300));

	//measure dialog
	m_measure_dlg = new MeasureDlg(this, this, wxID_ANY,
		wxDefaultPosition, wxSize(320, 300));

	//create annotation panel
	m_anno_view = new VAnnoView(this, this, wxID_ANY,
		wxDefaultPosition, wxSize(320, 300));

	//create prop panel
	m_prop_panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition, wxDefaultSize, 0, "PropPanel");
	//prop panel chidren
	m_prop_sizer = new wxBoxSizer(wxHORIZONTAL);
	m_volume_prop = new VPropView(this, m_prop_panel, wxID_ANY);
	m_mesh_prop = new MPropView(this, m_prop_panel, wxID_ANY);
	m_mesh_manip = new MManipulator(this, m_prop_panel, wxID_ANY);
	m_annotation_prop = new APropView(this, m_prop_panel, wxID_ANY);
	m_volume_prop->Show(false);
	m_mesh_prop->Show(false);
	m_mesh_manip->Show(false);
	m_annotation_prop->Show(false);

	//clipping view
	m_clip_view = new ClippingView(this, this, wxID_ANY,
		wxDefaultPosition, wxSize(130, 700));
	m_clip_view->SetDataManager(&m_data_mgr);

	//adjust view
	m_adjust_view = new AdjustView(this, this, wxID_ANY,
		wxDefaultPosition, wxSize(130, 700));

	wxString font_file = m_setting_dlg->GetFontFile();
	std::string exePath = wxStandardPaths::Get().GetExecutablePath().ToStdString();
	exePath = exePath.substr(0,exePath.find_last_of(std::string()+GETSLASH()));
	if (font_file != "")
		font_file = wxString(exePath) + GETSLASH() + wxString("Fonts") +
			GETSLASH() + font_file;
	else
		font_file = wxString(exePath) + GETSLASH() + wxString("Fonts") +
			GETSLASH() + wxString("FreeSans.ttf");
	m_text_renderer = new TextRenderer(font_file.ToStdString());
	m_text_renderer->SetSize(m_setting_dlg->GetTextSize());

	//settings dialog
	if (m_setting_dlg->GetTestMode(1))
		m_vrv_list[0]->m_glview->m_test_speed = true;
	if (m_setting_dlg->GetTestMode(3))
	{
		m_vrv_list[0]->m_glview->m_test_wiref = true;
		m_vrv_list[0]->m_glview->m_draw_bounds = true;
		m_vrv_list[0]->m_glview->m_draw_grid = true;
		m_data_mgr.m_vol_test_wiref = true;
	}
	int c1 = m_setting_dlg->GetWavelengthColor(1);
	int c2 = m_setting_dlg->GetWavelengthColor(2);
	int c3 = m_setting_dlg->GetWavelengthColor(3);
	int c4 = m_setting_dlg->GetWavelengthColor(4);
	if (c1 && c2 && c3 && c4)
		m_data_mgr.SetWavelengthColor(c1, c2, c3, c4);
	m_vrv_list[0]->SetPeelingLayers(m_setting_dlg->GetPeelingLyers());
	m_vrv_list[0]->SetBlendSlices(m_setting_dlg->GetMicroBlend());
	m_vrv_list[0]->SetAdaptive(m_setting_dlg->GetMouseInt());
	m_vrv_list[0]->SetGradBg(m_setting_dlg->GetGradBg());
	m_vrv_list[0]->SetPointVolumeMode(m_setting_dlg->GetPointVolumeMode());
	m_vrv_list[0]->SetRulerUseTransf(m_setting_dlg->GetRulerUseTransf());
	m_vrv_list[0]->SetRulerTimeDep(m_setting_dlg->GetRulerTimeDep());
	m_vrv_list[0]->SetTextRenderer(m_text_renderer);
	m_time_id = m_setting_dlg->GetTimeId();
	m_data_mgr.SetOverrideVox(m_setting_dlg->GetOverrideVox());
	m_data_mgr.SetPvxmlFlipX(m_setting_dlg->GetPvxmlFlipX());
	m_data_mgr.SetPvxmlFlipY(m_setting_dlg->GetPvxmlFlipY());
	VolumeRenderer::set_soft_threshold(m_setting_dlg->GetSoftThreshold());
	MultiVolumeRenderer::set_soft_threshold(m_setting_dlg->GetSoftThreshold());
	TreeLayer::SetSoftThreshsold(m_setting_dlg->GetSoftThreshold());
	VolumeMeshConv::SetSoftThreshold(m_setting_dlg->GetSoftThreshold());

	//brush tool dialog
	m_brush_tool_dlg = new BrushToolDlg(this, this);

	//noise cancelling dialog
	m_noise_cancelling_dlg = new NoiseCancellingDlg(this, this);

	//counting dialog
	m_counting_dlg = new CountingDlg(this, this);

	//convert dialog
	m_convert_dlg = new ConvertDlg(this, this);

	//colocalization dialog
	m_colocalization_dlg = new ColocalizationDlg(this, this);

	//trace dialog
	m_trace_dlg = new TraceDlg(this, this);

	//database dialog
#ifdef WITH_DATABASE
	m_database_dlg = new DatabaseDlg(this, this);
#endif // WITH_DATABASE

	//help dialog
	m_help_dlg = new HelpDlg(this, this);
	//m_help_dlg->LoadPage("C:\\!wanyong!\\TEMP\\wxHtmlWindow.htm");

	//tester
	//shown for testing parameters
	m_tester = new TesterDlg(this, this);
	if (m_setting_dlg->GetTestMode(2))
		m_tester->Show(true);
	else
		m_tester->Show(false);

	//Add to the manager
	m_aui_mgr.AddPane(m_main_tb, wxAuiPaneInfo().
		Name("m_main_tb").Caption("Toolbar").CaptionVisible(false).
		MinSize(wxSize(-1, 49)).MaxSize(wxSize(-1, 50)).
		Top().CloseButton(false).Layer(4));
	m_aui_mgr.AddPane(m_tree_panel, wxAuiPaneInfo().
		Name("m_tree_panel").Caption(UITEXT_TREEVIEW).
		Left().CloseButton(true).BestSize(wxSize(320, 300)).
		FloatingSize(wxSize(320, 300)).Layer(3));
	m_aui_mgr.AddPane(m_measure_dlg, wxAuiPaneInfo().
		Name("m_measure_dlg").Caption(UITEXT_MEASUREMENT).
		Left().CloseButton(true).BestSize(wxSize(320, 400)).
		FloatingSize(wxSize(320, 300)).Layer(3));
	m_aui_mgr.AddPane(m_movie_view, wxAuiPaneInfo().
		Name("m_movie_view").Caption(UITEXT_MAKEMOVIE).
		Left().CloseButton(true).MinSize(wxSize(320, 300)).
		FloatingSize(wxSize(320, 300)).Layer(3));
	m_aui_mgr.AddPane(m_prop_panel, wxAuiPaneInfo().
		Name("m_prop_panel").Caption(UITEXT_PROPERTIES).
		Bottom().CloseButton(true).MinSize(wxSize(300, 150)).
		FloatingSize(wxSize(1100, 130)).Layer(2));
	m_aui_mgr.AddPane(m_adjust_view, wxAuiPaneInfo().
		Name("m_adjust_view").Caption(UITEXT_ADJUST).
		Left().CloseButton(true).MinSize(wxSize(110, 700)).
		FloatingSize(wxSize(110, 700)).Layer(1));
	m_aui_mgr.AddPane(m_clip_view, wxAuiPaneInfo().
		Name("m_clip_view").Caption(UITEXT_CLIPPING).
		Right().CloseButton(true).MinSize(wxSize(130, 700)).
		FloatingSize(wxSize(130, 700)).Layer(1));
	m_aui_mgr.AddPane(vrv, wxAuiPaneInfo().
		Name(vrv->GetName()).Caption(vrv->GetName()).
		Dockable(true).CloseButton(false).
		FloatingSize(wxSize(600, 400)).MinSize(wxSize(300, 200)).
		Layer(0).Centre());

	m_aui_mgr.GetPane("m_tree_panel").dock_proportion  = 30;
	m_aui_mgr.GetPane("m_measure_dlg").dock_proportion = 20;
	m_aui_mgr.GetPane("m_movie_view").dock_proportion = 10;

	m_aui_mgr.GetPane(m_measure_dlg).Float();
	m_aui_mgr.GetPane(m_measure_dlg).Hide();

	//dialogs
	//brush tool dialog
	m_aui_mgr.AddPane(m_brush_tool_dlg, wxAuiPaneInfo().
		Name("m_brush_tool_dlg").Caption("Analyze").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_brush_tool_dlg).Float();
	m_aui_mgr.GetPane(m_brush_tool_dlg).Hide();
	//noise cancelling dialog
	m_aui_mgr.AddPane(m_noise_cancelling_dlg, wxAuiPaneInfo().
		Name("m_noise_cancelling_dlg").Caption("Noise Reduction").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_noise_cancelling_dlg).Float();
	m_aui_mgr.GetPane(m_noise_cancelling_dlg).Hide();
	//counting dialog
	m_aui_mgr.AddPane(m_counting_dlg, wxAuiPaneInfo().
		Name("m_counting_dlg").Caption("Counting and Volume").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_counting_dlg).Float();
	m_aui_mgr.GetPane(m_counting_dlg).Hide();
	//convert dialog
	m_aui_mgr.AddPane(m_convert_dlg, wxAuiPaneInfo().
		Name("m_convert_dlg").Caption("Convert").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_convert_dlg).Float();
	m_aui_mgr.GetPane(m_convert_dlg).Hide();
	//colocalization dialog
	m_aui_mgr.AddPane(m_colocalization_dlg, wxAuiPaneInfo().
		Name("m_colocalization_dlg").Caption("Colocalization Analysis").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_colocalization_dlg).Float();
	m_aui_mgr.GetPane(m_colocalization_dlg).Hide();
	//trace dialog
	m_aui_mgr.AddPane(m_trace_dlg, wxAuiPaneInfo().
		Name("m_trace_dlg").Caption("Components & Tracking").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_trace_dlg).Float();
	m_aui_mgr.GetPane(m_trace_dlg).Hide();
	//information dialog
	m_aui_mgr.AddPane(m_anno_view, wxAuiPaneInfo().
		Name("m_anno_view").Caption("Information").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_anno_view).Float();
	m_aui_mgr.GetPane(m_anno_view).Hide();
	//database dialog
#ifdef WITH_DATABASE
	m_aui_mgr.AddPane(m_database_dlg, wxAuiPaneInfo().
		Name("m_database_dlg").Caption("Database").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_database_dlg).Float();
	m_aui_mgr.GetPane(m_database_dlg).Hide();
#endif // WITH_DATABASE
	//settings
	m_aui_mgr.AddPane(m_setting_dlg, wxAuiPaneInfo().
		Name("m_setting_dlg").Caption("Settings").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_setting_dlg).Float();
	m_aui_mgr.GetPane(m_setting_dlg).Hide();
	//help
	m_aui_mgr.AddPane(m_help_dlg, wxAuiPaneInfo().
		Name("m_help_dlg").Caption("Help").
		Dockable(false).CloseButton(true));
	m_aui_mgr.GetPane(m_help_dlg).Float();
	m_aui_mgr.GetPane(m_help_dlg).Hide();


	UpdateTree();

	SetMinSize(wxSize(800,600));

	m_aui_mgr.Update();
	Maximize();

	//make movie settings
	m_mov_view = 0;
	m_mov_axis = 1;
	m_mov_rewind = false;

	//set view default settings
	if (m_adjust_view && vrv)
	{
		Color gamma, brightness, hdr;
		bool sync_r, sync_g, sync_b;
		m_adjust_view->GetDefaults(gamma, brightness, hdr,
			sync_r, sync_g, sync_b);
		vrv->m_glview->SetGamma(gamma);
		vrv->m_glview->SetBrightness(brightness);
		vrv->m_glview->SetHdr(hdr);
		vrv->m_glview->SetSyncR(sync_r);
		vrv->m_glview->SetSyncG(sync_g);
		vrv->m_glview->SetSyncB(sync_b);
	}

	TextureRenderer::set_mainmem_buf_size(m_setting_dlg->GetMainMemBufSize());
	TextureRenderer::set_available_mainmem_buf_size(m_setting_dlg->GetMainMemBufSize());

	//drop target
	SetDropTarget(new DnDFile(this));

#if wxUSE_STATUSBAR
	CreateStatusBar(2);
	GetStatusBar()->SetStatusText(wxString(FLUORENDER_TITLE)+
		wxString(" started normally."));
#endif // wxUSE_STATUSBAR

	//main top menu
	m_top_menu = new wxMenuBar;
	m_top_file = new wxMenu;
	m_top_tools = new wxMenu;
	m_top_window = new wxMenu;
	//m_top_help = new wxMenu;
	//file options
	wxMenuItem *m = new wxMenuItem(m_top_file,ID_OpenVolume, wxT("Open &Volume"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_open_volume_mini));
	m_top_file->Append(m);
	m = new wxMenuItem(m_top_file,ID_OpenMesh, wxT("Open &Mesh"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_open_mesh_mini));
	m_top_file->Append(m);
	m = new wxMenuItem(m_top_file,ID_OpenProject, wxT("Open &Project"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_open_project_mini));
	m_top_file->Append(m);
	m = new wxMenuItem(m_top_file,ID_SaveProject, wxT("&Save Project"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_save_project_mini));
	m_top_file->Append(m);
	m_top_file->Append(wxID_SEPARATOR);
	wxMenuItem *quit = new wxMenuItem(m_top_file, wxID_EXIT);
	quit->SetBitmap(wxArtProvider::GetBitmap(wxART_QUIT));
	m_top_file->Append(quit);
	//tool options
	m = new wxMenuItem(m_top_tools,ID_PaintTool, wxT("&Analysis Tools..."));
	m->SetBitmap(wxGetBitmapFromMemory(icon_edit_mini));
	m_top_tools->Append(m);
	m = new wxMenuItem(m_top_tools,ID_Measure, wxT("&Measurement Tools..."));
	m_top_tools->Append(m);
	m = new wxMenuItem(m_top_tools,ID_Trace, wxT("Components && &Tracking..."));
	m_top_tools->Append(m);
	m = new wxMenuItem(m_top_tools,ID_NoiseCancelling, wxT("&Noise Reduction..."));
	m_top_tools->Append(m);
	m = new wxMenuItem(m_top_tools,ID_Counting, wxT("&Counting and Volume..."));
	m_top_tools->Append(m);
	m = new wxMenuItem(m_top_tools,ID_Colocalization, wxT("Colocalization &Analysis..."));
	m_top_tools->Append(m);
	m = new wxMenuItem(m_top_tools,ID_Convert, wxT("Con&vert..."));
	m_top_tools->Append(m);
	m_top_tools->Append(wxID_SEPARATOR);
	m = new wxMenuItem(m_top_tools,ID_Settings, wxT("&Settings..."));
	m->SetBitmap(wxGetBitmapFromMemory(icon_settings_mini));
	m_top_tools->Append(m);
	//window option
	m = new wxMenuItem(m_top_window,ID_ShowHideToolbar, wxT("Show/Hide &Toolbar"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_ShowHideToolbar, true);
	m_top_window->Append(wxID_SEPARATOR);
	m = new wxMenuItem(m_top_window,ID_ShowHideUI, wxT("Show/Hide &UI"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_show_hide_ui_mini));
	m_top_window->Append(m);
	m = new wxMenuItem(m_top_window,ID_UIListView, wxT("&Datasets"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_UIListView, true);
	m = new wxMenuItem(m_top_window,ID_UITreeView, wxT("&Workspace"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_UITreeView, true);
	m = new wxMenuItem(m_top_window,ID_UIMeasureView, wxT("&Measurement"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_UIMeasureView, true);
	m = new wxMenuItem(m_top_window,ID_UIMovieView, wxT("&Recorder"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_UIMovieView, true);
	m = new wxMenuItem(m_top_window,ID_UIAdjView, wxT("&Output Adjustments"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_UIAdjView, true);
	m = new wxMenuItem(m_top_window,ID_UIClipView, wxT("&Clipping Planes"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_UIClipView, true);
	m = new wxMenuItem(m_top_window,ID_UIPropView, wxT("&Properties"), wxEmptyString, wxITEM_CHECK);
	m_top_window->Append(m);
	m_top_window->Check(ID_UIPropView, true);
	m_top_window->Append(wxID_SEPARATOR);
	m = new wxMenuItem(m_top_window,ID_ViewNew, wxT("&New View"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_new_view_mini));
	m_top_window->Append(m);
#ifndef _DARWIN
	m = new wxMenuItem(m_top_window, ID_FullScreen, wxT("&Full Screen"));
	m->SetBitmap(wxGetBitmapFromMemory(full_screen_menu));
	m_top_window->Append(m);
#endif
	//help menu
/*	m = new wxMenuItem(m_top_help,ID_CheckUpdates, wxT("&Check for Updates"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_check_updates_mini));
	m_top_help->Append(m);
	m = new wxMenuItem(m_top_help,ID_Twitter, wxT("&Twitter"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_twitter_mini));
	m_top_help->Append(m);
	m = new wxMenuItem(m_top_help,ID_Facebook, wxT("&Facebook"));
	m->SetBitmap(wxGetBitmapFromMemory(icon_facebook_mini));
	m_top_help->Append(m);
	m = new wxMenuItem(m_top_help,ID_Info, wxT("&About FluoRender..."));
	m->SetBitmap(wxGetBitmapFromMemory(icon_about_mini));
	m_top_help->Append(m);
*/	//add the menus
	m_top_menu->Append(m_top_file,wxT("&File"));
	m_top_menu->Append(m_top_tools,wxT("&Tools"));
	m_top_menu->Append(m_top_window,wxT("&Windows"));
//	m_top_menu->Append(m_top_help,wxT("&Help"));
//#ifdef _DARWIN
	SetMenuBar(m_top_menu);
//#endif
/*
	m_aui_mgr.GetPane(m_main_tb).Hide();
	m_top_window->Check(ID_ShowHideToolbar,false);
	m_aui_mgr.Update();
*/
	m_tree_panel->ExpandAll();

	Thaw();
	SetEvtHandlerEnabled(true);
}

VRenderFrame::~VRenderFrame()
{
	for (int i=0; i<(int)m_vrv_list.size(); i++)
	{
		VRenderView* vrv = m_vrv_list[i];
		if (vrv) vrv->Clear();
	}
	if (m_text_renderer)
		delete m_text_renderer;
	m_aui_mgr.UnInit();

	curl_easy_cleanup(_g_curl);//add by takashi
	curl_multi_cleanup(_g_curlm);
	curl_global_cleanup();//add by takashi

	TextureBrick::delete_all_cache_files();
}

void VRenderFrame::OnExit(wxCommandEvent& WXUNUSED(event))
{
	Close(true);
}

void VRenderFrame::OnClose(wxCloseEvent &event)
{
	m_setting_dlg->SaveSettings();
/*	bool vrv_saved = false;
	for (unsigned int i=0; i<m_vrv_list.size(); ++i)
	{
		if (m_vrv_list[i]->m_default_saved)
		{
			vrv_saved = true;
			break;
		}
	}
	if (!vrv_saved)
		m_vrv_list[0]->SaveDefault(0xaff);
*/	event.Skip();
}

wxString VRenderFrame::CreateView(int row)
{
	VRenderView* vrv = 0;
	if (m_vrv_list.size()>0)
	{
		wxGLContext* sharedContext = m_vrv_list[0]->GetContext();
		vrv = new VRenderView(this, this, wxID_ANY, sharedContext);
		m_aui_mgr.AddPane(vrv, wxAuiPaneInfo().
			Name(vrv->GetName()).Caption(vrv->GetName()).
			Dockable(true).CloseButton(false).
			FloatingSize(wxSize(600, 400)).MinSize(wxSize(300, 200)).
			Layer(0).Centre().Right());
	}
	else
	{
		vrv = new VRenderView(this, this, wxID_ANY);
		m_aui_mgr.AddPane(vrv, wxAuiPaneInfo().
			Name(vrv->GetName()).Caption(vrv->GetName()).
			Dockable(true).CloseButton(false).
			FloatingSize(wxSize(600, 400)).MinSize(wxSize(300, 200)).
			Layer(0).Centre());
	}

	if (vrv)
	{
		vrv->SetDropTarget(new DnDFile(this, vrv));
		m_vrv_list.push_back(vrv);
		if (m_movie_view)
			m_movie_view->AddView(vrv->GetName());
		if (m_setting_dlg->GetTestMode(3))
		{
			vrv->m_glview->m_test_wiref = true;
			vrv->m_glview->m_draw_bounds = true;
			vrv->m_glview->m_draw_grid = true;
		}
		vrv->SetPeelingLayers(m_setting_dlg->GetPeelingLyers());
		vrv->SetBlendSlices(m_setting_dlg->GetMicroBlend());
		vrv->SetAdaptive(m_setting_dlg->GetMouseInt());
		vrv->SetGradBg(m_setting_dlg->GetGradBg());
		vrv->SetPointVolumeMode(m_setting_dlg->GetPointVolumeMode());
		vrv->SetRulerUseTransf(m_setting_dlg->GetRulerUseTransf());
		vrv->SetRulerTimeDep(m_setting_dlg->GetRulerTimeDep());
		vrv->SetTextRenderer(m_text_renderer);
	}

	//m_aui_mgr.Update();
	OrganizeVRenderViews(1);
	UpdateTree();

	//set view default settings
	if (m_adjust_view && vrv)
	{
		Color gamma, brightness, hdr;
		bool sync_r, sync_g, sync_b;
		m_adjust_view->GetDefaults(gamma, brightness, hdr, sync_r, sync_g, sync_b);
		vrv->m_glview->SetGamma(gamma);
		vrv->m_glview->SetBrightness(brightness);
		vrv->m_glview->SetHdr(hdr);
		vrv->m_glview->SetSyncR(sync_r);
		vrv->m_glview->SetSyncG(sync_g);
		vrv->m_glview->SetSyncB(sync_b);
	}

	if (vrv)
		return vrv->GetName();
	else
		return wxString("NO_NAME");
}

//views
int VRenderFrame::GetViewNum()
{
	return m_vrv_list.size();
}

vector <VRenderView*>* VRenderFrame::GetViewList()
{
	return &m_vrv_list;
}

VRenderView* VRenderFrame::GetView(int index)
{
	if (index>=0 && index<(int)m_vrv_list.size())
		return m_vrv_list[index];
	else
		return 0;
}

VRenderView* VRenderFrame::GetView(wxString& name)
{
	for (int i=0; i<(int)m_vrv_list.size(); i++)
	{
		VRenderView* vrv = m_vrv_list[i];
		if (vrv && vrv->GetName() == name)
		{
			return vrv;
		}
	}
	return 0;
}

void VRenderFrame::OnNewView(wxCommandEvent& WXUNUSED(event))
{
	wxString str = CreateView();
}

void VRenderFrame::OnFullScreen(wxCommandEvent& WXUNUSED(event))
{
	if (IsFullScreen())
	{
		ShowFullScreen(false);
		wxMenuItem* m = m_top_window->FindItem(ID_FullScreen);
		if (m)
			m->SetBitmap(wxGetBitmapFromMemory(full_screen_menu));
	}
	else
	{
		ShowFullScreen(true,
			wxFULLSCREEN_NOBORDER |
			wxFULLSCREEN_NOCAPTION);
		wxMenuItem* m = m_top_window->FindItem(ID_FullScreen);
		if (m)
			m->SetBitmap(wxGetBitmapFromMemory(full_screen_back_menu));
	}
}

//open dialog options
void VRenderFrame::OnCh1Check(wxCommandEvent &event)
{
	wxRadioButton* ch1 = (wxRadioButton*)event.GetEventObject();
	if (ch1)
		ch1->SetValue(m_sliceSequence);
}

void VRenderFrame::OnCh4Check(wxCommandEvent &event)
{
	wxRadioButton* ch4 = (wxRadioButton*)event.GetEventObject();
	if (ch4)
		ch4->SetValue(m_timeSequence);
}

void VRenderFrame::OnCh1Click(wxEvent &event)
{
	wxRadioButton* ch1 = (wxRadioButton*)event.GetEventObject();
	if (ch1)
	{
		bool bval = ch1->GetValue();
		if (bval)
		{
			ch1->SetValue(false);
			m_sliceSequence = false;
		}
		else
		{
			ch1->SetValue(true);
			m_sliceSequence = true;
			m_timeSequence = false;
		}
	}
}

void VRenderFrame::OnCh4Click(wxEvent &event)
{
	wxRadioButton* ch4 = (wxRadioButton*)event.GetEventObject();
	if (ch4)
	{
		bool bval = ch4->GetValue();
		if (bval)
		{
			ch4->SetValue(false);
			m_timeSequence = false;
		}
		else
		{
			ch4->SetValue(true);
			m_sliceSequence = false;
			m_timeSequence = true;
		}
	}
}

void VRenderFrame::OnTxt1Change(wxCommandEvent &event)
{
	wxTextCtrl* txt1 = (wxTextCtrl*)event.GetEventObject();
	if (txt1)
		m_time_id = txt1->GetValue();
}

void VRenderFrame::OnCh2Check(wxCommandEvent &event)
{
	wxCheckBox* ch2 = (wxCheckBox*)event.GetEventObject();
	if (ch2)
		m_compression = ch2->GetValue();
}

void VRenderFrame::OnCh3Check(wxCommandEvent &event)
{
	wxCheckBox* ch3 = (wxCheckBox*)event.GetEventObject();
	if (ch3)
		m_skip_brick = ch3->GetValue();
}

wxWindow* VRenderFrame::CreateExtraControlVolume(wxWindow* parent)
{
	wxPanel* panel = new wxPanel(parent, 0, wxDefaultPosition, wxSize(640, 110));

	wxBoxSizer *group1 = new wxStaticBoxSizer(
		new wxStaticBox(panel, wxID_ANY, "Additional Options"), wxVERTICAL );

	//slice sequence check box
	wxRadioButton* ch1 = new wxRadioButton(panel, wxID_HIGHEST+3001,
		"Read a sequence as Z slices (the last digits in filenames are used to identify the sequence)",
		wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	//time sequence check box
	wxRadioButton* ch4 = new wxRadioButton(panel, wxID_HIGHEST+3007,
		"Read a sequence as time series (the last digits in filenames are used to identify the sequence)",
		wxDefaultPosition, wxDefaultSize);

	ch1->Connect(ch1->GetId(), wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(VRenderFrame::OnCh1Check), NULL, panel);
	ch1->Connect(ch1->GetId(), wxEVT_LEFT_DOWN,
		wxEventHandler(VRenderFrame::OnCh1Click), NULL, panel);
	ch1->SetValue(m_sliceSequence);

	ch4->Connect(ch4->GetId(), wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(VRenderFrame::OnCh4Check), NULL, panel);
	ch4->Connect(ch4->GetId(), wxEVT_LEFT_DOWN,
		wxEventHandler(VRenderFrame::OnCh4Click), NULL, panel);
	ch4->SetValue(m_timeSequence);

	//compression
	wxCheckBox* ch2 = new wxCheckBox(panel, wxID_HIGHEST+3002,
		"Compress data (loading will take longer time and data are compressed in graphics memory)");
	ch2->Connect(ch2->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(VRenderFrame::OnCh2Check), NULL, panel);
	ch2->SetValue(m_compression);

	//empty brick skipping
	wxCheckBox* ch3 = new wxCheckBox(panel, wxID_HIGHEST+3006,
		"Skip empty bricks during rendering (loading takes longer time)");
	ch3->Connect(ch3->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(VRenderFrame::OnCh3Check), NULL, panel);
	ch3->SetValue(m_skip_brick);

	//time sequence identifier
	wxBoxSizer* sizer1 = new wxBoxSizer(wxHORIZONTAL);
	wxTextCtrl* txt1 = new wxTextCtrl(panel, wxID_HIGHEST+3003,
		"", wxDefaultPosition, wxSize(80, 20));
	txt1->SetValue(m_time_id);
	txt1->Connect(txt1->GetId(), wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(VRenderFrame::OnTxt1Change), NULL, panel);
	wxStaticText* st = new wxStaticText(panel, 0,
		"Sequence identifier (digits after the identifier in filenames are used as index)");
	sizer1->Add(txt1);
	sizer1->Add(10, 10);
	sizer1->Add(st);

	group1->Add(10, 10);
	group1->Add(ch1);
	group1->Add(10, 10);
	group1->Add(ch4);
	group1->Add(10, 10);
	group1->Add(ch2);
	group1->Add(10, 10);
	group1->Add(ch3);
	group1->Add(10, 10);
	group1->Add(sizer1);
	group1->Add(10, 10);

	panel->SetSizerAndFit(group1);
	panel->Layout();

	return panel;
}

void VRenderFrame::OnOpenVolume(wxCommandEvent& WXUNUSED(event))
{
	if (m_setting_dlg)
	{
		m_compression = m_setting_dlg->GetRealtimeCompress();
		m_skip_brick = m_setting_dlg->GetSkipBricks();
	}

	wxFileDialog *fopendlg = new wxFileDialog(
		this, "Choose the volume data file", "", "",
		"All Supported|*.tif;*.tiff;*.oib;*.oif;*.lsm;*.xml;*.nrrd;*.vvd|"\
		"Tiff Files (*.tif, *.tiff)|*.tif;*.tiff|"\
		"Olympus Image Binary Files (*.oib)|*.oib|"\
		"Olympus Original Imaging Format (*.oif)|*.oif|"\
		"Zeiss Laser Scanning Microscope (*.lsm)|*.lsm|"\
		"Prairie View XML (*.xml)|*.xml|"\
		"Nrrd files (*.nrrd)|*.nrrd|"\
		"VVD files (*.vvd)|*.vvd", wxFD_OPEN|wxFD_MULTIPLE);
	fopendlg->SetExtraControlCreator(CreateExtraControlVolume);

	int rval = fopendlg->ShowModal();
	if (rval == wxID_OK)
	{
		VRenderView* vrv = GetView(0);

		wxArrayString paths;
		fopendlg->GetPaths(paths);
		LoadVolumes(paths, vrv);

		if (m_setting_dlg)
		{
			m_setting_dlg->SetRealtimeCompress(m_compression);
			m_setting_dlg->SetSkipBricks(m_skip_brick);
			m_setting_dlg->UpdateUI();
		}
	}

	fopendlg->Destroy();
}

void VRenderFrame::OnDownloadVolume(wxCommandEvent& WXUNUSED(event))
{
	m_aui_mgr.GetPane(m_database_dlg).Show();
	m_aui_mgr.GetPane(m_database_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::LoadVolumes(wxArrayString files, VRenderView* view, vector<vector<AnnotationDB>> annotations)
{
	int j;

	VolumeData* vd_sel = 0;
	DataGroup* group_sel = 0;
	VRenderView* vrv = 0;

	if (view)
		vrv = view;
	else
		vrv = GetView(0);

	wxProgressDialog *prg_diag = 0;
	if (vrv)
	{
		prg_diag = new wxProgressDialog(
			"FluoRender: Loading volume data...",
			"Reading and processing selected volume data. Please wait.",
			100, 0, wxPD_SMOOTH|wxPD_ELAPSED_TIME|wxPD_AUTO_HIDE);

		m_data_mgr.SetSliceSequence(m_sliceSequence);
		m_data_mgr.SetTimeSequence(m_timeSequence);
		m_data_mgr.SetCompression(m_compression);
		m_data_mgr.SetSkipBrick(m_skip_brick);
		m_data_mgr.SetTimeId(m_time_id);
		m_data_mgr.SetLoadMask(m_load_mask);
		m_setting_dlg->SetTimeId(m_time_id);

		bool enable_4d = false;

		for (j=0; j<(int)files.Count(); j++)
		{
			prg_diag->Update(90*(j+1)/(int)files.Count(),
				"FluoRender: Loading volume data...");

			int ch_num = 0;
			wxString filename = files[j];
			wxString suffix = filename.Mid(filename.Find('.', true)).MakeLower();

			if (suffix == ".nrrd")
				ch_num = m_data_mgr.LoadVolumeData(filename, LOAD_TYPE_NRRD);
			else if (suffix==".tif" || suffix==".tiff")
				ch_num = m_data_mgr.LoadVolumeData(filename, LOAD_TYPE_TIFF);
			else if (suffix == ".oib")
				ch_num = m_data_mgr.LoadVolumeData(filename, LOAD_TYPE_OIB);
			else if (suffix == ".oif")
				ch_num = m_data_mgr.LoadVolumeData(filename, LOAD_TYPE_OIF);
			else if (suffix==".lsm")
				ch_num = m_data_mgr.LoadVolumeData(filename, LOAD_TYPE_LSM);
			else if (suffix==".xml")
				ch_num = m_data_mgr.LoadVolumeData(filename, LOAD_TYPE_PVXML);
			else if (suffix==".vvd")
				ch_num = m_data_mgr.LoadVolumeData(filename, LOAD_TYPE_BRKXML);

			if (ch_num > 1)
			{
				wxString group_name = vrv->AddGroup();
				DataGroup* group = vrv->GetGroup(group_name);
				if (group)
				{
					for (int i=ch_num; i>0; i--)
					{
						VolumeData* vd = m_data_mgr.GetVolumeData(m_data_mgr.GetVolumeNum()-i);
						if (vd)
						{
							vrv->AddVolumeData(vd);
							wxString vol_name = vd->GetName();
							if (vol_name.Find("_1ch")!=-1 &&
								(i==1 || i==2))
								vd->SetDisp(false);
							if (vol_name.Find("_2ch")!=-1 && i==1)
								vd->SetDisp(false);

							if (i==ch_num)
							{
								vd_sel = vd;
								group_sel = group;
							}

							if (vd->GetReader() && vd->GetReader()->GetTimeNum()>1)
								enable_4d = true;

							if(files.Count() == annotations.size())vd->SetAnnotation(annotations[j]);
						}
					}
					if (j > 0)
						group->SetDisp(false);
				}
			}
			else if (ch_num == 1)
			{
				VolumeData* vd = m_data_mgr.GetVolumeData(m_data_mgr.GetVolumeNum()-1);
				if (vd)
				{
					int chan_num = vrv->GetDispVolumeNum();
					Color color(1.0, 1.0, 1.0);
					if (chan_num == 0)
						color = Color(1.0, 0.0, 0.0);
					else if (chan_num == 1)
						color = Color(0.0, 1.0, 0.0);
					else if (chan_num == 2)
						color = Color(0.0, 0.0, 1.0);

					if (chan_num >=0 && chan_num <3)
						vd->SetColor(color);
					else
						vd->RandomizeColor();

					vrv->AddVolumeData(vd);
					vd_sel = vd;

					if (vd->GetReader() && vd->GetReader()->GetTimeNum()>1){
						vrv->m_glview->m_tseq_cur_num = vd->GetReader()->GetCurTime();
						enable_4d = true;
					}

					if(files.Count() == annotations.size())vd->SetAnnotation(annotations[j]);
				}
			}
		}
		if (vd_sel)
			UpdateTree(vd_sel->GetName(), 2);
		else
			UpdateTree();

		if (vrv)
			vrv->InitView(INIT_BOUNDS|INIT_CENTER);
		if (enable_4d) {
			m_movie_view->EnableTime();
			m_movie_view->DisableRot();
			m_movie_view->SetCurrentTime(vrv->m_glview->m_tseq_cur_num);
		}
		delete prg_diag;
	}

	vrv->RefreshGL();//added by Takashi
}

void VRenderFrame::StartupLoad(wxArrayString files)
{
	if (m_vrv_list[0])
		m_vrv_list[0]->m_glview->Init();

	if (files.Count())
	{
		wxString filename = files[0];
		wxString suffix = filename.Mid(filename.Find('.', true)).MakeLower();

		if (suffix == ".vrp")
		{
			OpenProject(files[0]);
		}
		else if (suffix == ".nrrd" ||
			suffix == ".tif" ||
			suffix == ".tiff" ||
			suffix == ".oib" ||
			suffix == ".oif" ||
			suffix == ".lsm" ||
			suffix == ".xml" ||
			suffix == ".vvd" )
		{
			LoadVolumes(files);
		}
		else if (suffix == ".obj" || suffix == ".swc")
		{
			LoadMeshes(files);
		}
	}
}

void VRenderFrame::LoadMeshes(wxArrayString files, VRenderView* vrv)
{
	if (!vrv)
		vrv = GetView(0);

	MeshData* md_sel = 0;

	wxProgressDialog *prg_diag = new wxProgressDialog(
		"FluoRender: Loading mesh data...",
		"Reading and processing selected mesh data. Please wait.",
		100, 0, wxPD_SMOOTH|wxPD_ELAPSED_TIME|wxPD_AUTO_HIDE);

	MeshGroup* group = 0;
	if (files.Count() > 1)
	{
		wxString group_name = vrv->AddMGroup();
		group = vrv->GetMGroup(group_name);
	}

	for (int i=0; i<(int)files.Count(); i++)
	{
		prg_diag->Update(90*(i+1)/(int)files.Count());

		wxString filename = files[i];
		m_data_mgr.LoadMeshData(filename);

		MeshData* md = m_data_mgr.GetLastMeshData();
		if (vrv && md)
		{
			if (group)
			{
				group->InsertMeshData(group->GetMeshNum()-1, md);
				vrv->SetMeshPopDirty();
			}
			else
				vrv->AddMeshData(md);

			if (i==int(files.Count()-1))
				md_sel = md;
		}
	}

	if (md_sel)
		UpdateTree(md_sel->GetName(), 3);
	else
		UpdateTree();

	if (vrv)
		vrv->InitView(INIT_BOUNDS|INIT_CENTER);

	delete prg_diag;
}

void VRenderFrame::OnOpenMesh(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog *fopendlg = new wxFileDialog(
		this, "Choose the volume data file", "", "",
		"All Supported|*.obj;*.swc|OBJ files (*.obj)|*.obj|SWC files (*.swc)|*.swc",
		wxFD_OPEN|wxFD_MULTIPLE);

	int rval = fopendlg->ShowModal();
	if (rval == wxID_OK)
	{
		VRenderView* vrv = GetView(0);
		wxArrayString files;
		fopendlg->GetPaths(files);

		LoadMeshes(files, vrv);
	}

	fopendlg->Destroy();
}

void VRenderFrame::OnOrganize(wxCommandEvent& WXUNUSED(event))
{
	int w, h;
	GetClientSize(&w, &h);
	int view_num = m_vrv_list.size();
	if (view_num>1)
	{
		for (int i=0 ; i<view_num ; i++)
		{
			m_aui_mgr.GetPane(m_vrv_list[i]->GetName()).Float();
		}
		m_aui_mgr.Update();
	}
}

void VRenderFrame::OnCheckUpdates(wxCommandEvent &)
{
	::wxLaunchDefaultBrowser(VERSION_UPDATES);
}

void VRenderFrame::OnInfo(wxCommandEvent& WXUNUSED(event))
{

	wxString time = wxNow();
	int psJan = time.Find("Jan");
	int psDec = time.Find("Dec");
	wxDialog* d = new wxDialog(this,wxID_ANY,"About FluoRender",wxDefaultPosition,
		wxSize(540,210),wxDEFAULT_DIALOG_STYLE );
	wxBoxSizer * main = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer * left = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * right = new wxBoxSizer(wxVERTICAL);
	//left
	// FluoRender Image (rows 4-5)
	wxToolBar * logo= new wxToolBar(d, wxID_ANY,
		wxDefaultPosition, wxDefaultSize, wxTB_NODIVIDER);
	if (psJan!=wxNOT_FOUND || psDec!=wxNOT_FOUND)
		logo->AddTool(wxID_ANY, "", wxGetBitmapFromMemory(logo_snow));
	else
		logo->AddTool(wxID_ANY, "", wxGetBitmapFromMemory(logo));
	logo->Realize();
	left->Add(logo,0,wxEXPAND);
	//right
	wxStaticText *txt = new wxStaticText(d,wxID_ANY,FLUORENDER_TITLE,
		wxDefaultPosition,wxSize(-1,-1));
	wxFont font = wxFont(15,wxFONTFAMILY_ROMAN,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL );
	txt->SetFont(font);
	right->Add(txt,0,wxEXPAND);
	txt = new wxStaticText(d,wxID_ANY,"Version: " +
		wxString::Format("%d.%.1f", VERSION_MAJOR, float(VERSION_MINOR)),
		wxDefaultPosition,wxSize(50,-1));
	font = wxFont(12,wxFONTFAMILY_ROMAN,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL );
	txt->SetFont(font);
	right->Add(txt,0,wxEXPAND);
	txt = new wxStaticText(d,wxID_ANY,wxString("Copyright (c) ") + VERSION_COPYRIGHT,
		wxDefaultPosition,wxSize(-1,-1));
	font = wxFont(11,wxFONTFAMILY_ROMAN,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL );
	txt->SetFont(font);
	right->Add(txt,0,wxEXPAND);
	right->Add(3,5,0);
	txt = new wxStaticText(d,wxID_ANY,VERSION_AUTHORS,
		wxDefaultPosition,wxSize(-1,90));
	font = wxFont(10,wxFONTFAMILY_ROMAN,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL );
	txt->SetFont(font);
	right->Add(txt,0,wxEXPAND);
	wxHyperlinkCtrl* hyp = new wxHyperlinkCtrl(d,wxID_ANY,"Contact Info",
		VERSION_CONTACT,
		wxDefaultPosition,wxSize(-1,-1));
	right->Add(hyp,0,wxEXPAND);
	right->AddStretchSpacer();
	//put together
	main->Add(left,0,wxEXPAND);
	main->Add(right,0,wxEXPAND);
	d->SetSizer(main);
	d->ShowModal();
}

void VRenderFrame::UpdateTreeIcons()
{
	int i, j, k;
	if (!m_tree_panel || !m_tree_panel->GetTreeCtrl())
		return;

	m_tree_panel->SetEvtHandlerEnabled(false);
	m_tree_panel->Freeze();

	DataTreeCtrl* treectrl = m_tree_panel->GetTreeCtrl();
	wxTreeItemId root = treectrl->GetRootItem();
	wxTreeItemIdValue ck_view;
	
	for (i=0; i<(int)m_vrv_list.size(); i++)
	{
		VRenderView *vrv = m_vrv_list[i];
		wxTreeItemId vrv_item;
		if (i==0)
			vrv_item = treectrl->GetFirstChild(root, ck_view);
		else
			vrv_item = treectrl->GetNextChild(root, ck_view);

		if (!vrv_item.IsOk())
			continue;

		m_tree_panel->SetViewItemImage(vrv_item, vrv->GetDraw());

		wxTreeItemIdValue ck_layer;
		for (j=0; j<vrv->GetLayerNum(); j++)
		{
			TreeLayer* layer = vrv->GetLayer(j);
			wxTreeItemId layer_item;
			if (j==0)
				layer_item = treectrl->GetFirstChild(vrv_item, ck_layer);
			else
				layer_item = treectrl->GetNextChild(vrv_item, ck_layer);

			if (!layer_item.IsOk())
				continue;

			LayerInfo* item_data = (LayerInfo*)treectrl->GetItemData(layer_item);
			if (!item_data)
				continue;

			int iconid = item_data->icon / 2;

			switch (layer->IsA())
			{
			case 2://volume
				{
					VolumeData* vd = (VolumeData*)layer;
					if (!vd)
						break;
					m_tree_panel->SetVolItemImage(layer_item, vd->GetDisp()?2*iconid+1:2*iconid);
					if (vd->GetColormapMode() == 3)
						m_tree_panel->UpdateROITreeIcons(vd);
				}
				break;
			case 3://mesh
				{
					MeshData* md = (MeshData*)layer;
					if (!md)
						break;
					m_tree_panel->SetMeshItemImage(layer_item, md->GetDisp()?2*iconid+1:2*iconid);
				}
				break;
			case 4://annotations
				{
					Annotations* ann = (Annotations*)layer;
					if (!ann)
						break;
					m_tree_panel->SetAnnotationItemImage(layer_item, ann->GetDisp()?2*iconid+1:2*iconid);
				}
				break;
			case 5://volume group
				{
					DataGroup* group = (DataGroup*)layer;
					if (!group)
						break;
					m_tree_panel->SetGroupItemImage(layer_item, int(group->GetDisp()));
					wxTreeItemIdValue ck_volume;
					for (k=0; k<group->GetVolumeNum(); k++)
					{
						VolumeData* vd = group->GetVolumeData(k);
						if (!vd)
							continue;
						wxTreeItemId volume_item;
						if (k==0)
							volume_item = treectrl->GetFirstChild(layer_item, ck_volume);
						else
							volume_item = treectrl->GetNextChild(layer_item, ck_volume);
						if (!volume_item.IsOk())
							continue;
						item_data = (LayerInfo*)treectrl->GetItemData(volume_item);
						if (!item_data)
							continue;
						int iconid = item_data->icon / 2;
						m_tree_panel->SetVolItemImage(volume_item, vd->GetDisp()?2*iconid+1:2*iconid);
						if (vd->GetColormapMode() == 3)
							m_tree_panel->UpdateROITreeIcons(vd);
					}
				}
				break;
			case 6://mesh group
				{
					MeshGroup* group = (MeshGroup*)layer;
					if (!group)
						break;
					m_tree_panel->SetMGroupItemImage(layer_item, int(group->GetDisp()));
					wxTreeItemIdValue ck_mesh;
					for (k=0; k<group->GetMeshNum(); k++)
					{
						MeshData* md = group->GetMeshData(k);
						if (!md)
							continue;
						wxTreeItemId mesh_item;
						if (k==0)
							mesh_item = treectrl->GetFirstChild(layer_item, ck_mesh);
						else
							mesh_item = treectrl->GetNextChild(layer_item, ck_mesh);
						if (!mesh_item.IsOk())
							continue;
						m_tree_panel->SetMeshItemImage(mesh_item, md->GetDisp()?2*iconid+1:2*iconid);
					}
				}
				break;
			}
		}
	}

	m_tree_panel->Thaw();
	m_tree_panel->SetEvtHandlerEnabled(true);

	m_tree_panel->Refresh(false);
}

void VRenderFrame::UpdateTreeColors()
{
	int i, j, k;
	if (!m_tree_panel || !m_tree_panel->GetTreeCtrl())
		return;

	m_tree_panel->SetEvtHandlerEnabled(false);
	m_tree_panel->Freeze();

	DataTreeCtrl* treectrl = m_tree_panel->GetTreeCtrl();

	for (i=0 ; i<(int)m_vrv_list.size() ; i++)
	{
		VRenderView *vrv = m_vrv_list[i];

		for (j=0; j<vrv->GetLayerNum(); j++)
		{
			TreeLayer* layer = vrv->GetLayer(j);
			if (!layer)
				continue;

			wxString name = layer->GetName();
			
			wxTreeItemId layer_item = m_tree_panel->FindTreeItem(name);
			if (!layer_item.IsOk())
				continue;
			
			LayerInfo* item_data = (LayerInfo*)treectrl->GetItemData(layer_item);
			if (!item_data)
				continue;
			
			int iconid = item_data->icon / 2;

			switch (layer->IsA())
			{
			case 0://root
				break;
			case 1://view
				break;
			case 2://volume
				{
					VolumeData* vd = (VolumeData*)layer;
					if (!vd)
						break;
					Color c = vd->GetColor();
					wxColor wxc(
						(unsigned char)(c.r()*255),
						(unsigned char)(c.g()*255),
						(unsigned char)(c.b()*255));
					m_tree_panel->ChangeIconColor(iconid, wxc);
					if (vd->GetColormapMode() == 3)
						m_tree_panel->UpdateROITreeIconColor(vd);
				}
				break;
			case 3://mesh
				{
					MeshData* md = (MeshData*)layer;
					if (!md)
						break;
					Color amb, diff, spec;
					double shine, alpha;
					md->GetMaterial(amb, diff, spec, shine, alpha);
					wxColor wxc(
						(unsigned char)(diff.r()*255),
						(unsigned char)(diff.g()*255),
						(unsigned char)(diff.b()*255));
					m_tree_panel->ChangeIconColor(iconid, wxc);
				}
				break;
			case 4://annotations
				{
					Annotations* ann = (Annotations*)layer;
					if (!ann)
						break;
					wxColor wxc(255, 255, 255);
					m_tree_panel->ChangeIconColor(iconid, wxc);
				}
				break;
			case 5://group
				{
					DataGroup* group = (DataGroup*)layer;
					if (!group)
						break;
					for (k=0; k<group->GetVolumeNum(); k++)
					{
						VolumeData* vd = group->GetVolumeData(k);
						if (!vd)
							break;
						
						wxString v_name = vd->GetName();
						wxTreeItemId v_item = m_tree_panel->FindTreeItem(v_name);
						if (!v_item.IsOk())
							continue;
						LayerInfo* vitem_data = (LayerInfo*)treectrl->GetItemData(v_item);
						if (!vitem_data)
							continue;
						iconid = vitem_data->icon / 2;

						Color c = vd->GetColor();
						wxColor wxc(
							(unsigned char)(c.r()*255),
							(unsigned char)(c.g()*255),
							(unsigned char)(c.b()*255));
						m_tree_panel->ChangeIconColor(iconid, wxc);
						if (vd->GetColormapMode() == 3)
							m_tree_panel->UpdateROITreeIconColor(vd);
					}
				}
				break;
			case 6://mesh group
				{
					MeshGroup* group = (MeshGroup*)layer;
					if (!group)
						break;
					for (k=0; k<group->GetMeshNum(); k++)
					{
						MeshData* md = group->GetMeshData(k);
						if (!md)
							break;
						Color amb, diff, spec;
						double shine, alpha;
						md->GetMaterial(amb, diff, spec, shine, alpha);
						wxColor wxc(
							(unsigned char)(diff.r()*255),
							(unsigned char)(diff.g()*255),
							(unsigned char)(diff.b()*255));
						m_tree_panel->ChangeIconColor(iconid, wxc);
					}
				}
				break;
			}
		}
	}
	
	m_tree_panel->Thaw();
	m_tree_panel->SetEvtHandlerEnabled(true);

	m_tree_panel->Refresh(false);
}

void VRenderFrame::UpdateTree(wxString name, int type, bool set_calc)
{
	if (!m_tree_panel)
		return;

	m_tree_panel->SaveExpState();
	int scroll_pos;
	scroll_pos = m_tree_panel->GetScrollPos(wxVERTICAL);

	m_tree_panel->SetEvtHandlerEnabled(false);
	m_tree_panel->Freeze();

	m_tree_panel->DeleteAll();
	m_tree_panel->ClearIcons();

	wxString root_str = "Active Datasets";
	wxTreeItemId root_item = m_tree_panel->AddRootItem(root_str);
	if (name == root_str)
		m_tree_panel->SelectItem(root_item);
	//append non-color icons for views
	m_tree_panel->AppendIcon();
	m_tree_panel->Expand(root_item);
	m_tree_panel->ChangeIconColor(0, wxColor(255, 255, 255));

	vector<int> used_vols;
	int dm_vnum = m_data_mgr.GetVolumeNum();
	if (dm_vnum > 0) used_vols.resize(dm_vnum, 0);

	wxTreeItemId sel_item;

	for (int i=0 ; i<(int)m_vrv_list.size() ; i++)
	{
		int j, k;
		VRenderView *vrv = m_vrv_list[i];
		if (!vrv)
			continue;

		vrv->OrganizeLayers();
		wxTreeItemId vrv_item = m_tree_panel->AddViewItem(vrv->GetName());
		m_tree_panel->SetViewItemImage(vrv_item, vrv->GetDraw());
		if (name == vrv->GetName() && (type == 1 || type < 0))
			m_tree_panel->SelectItem(vrv_item);

		for (j=0; j<vrv->GetLayerNum(); j++)
		{
			TreeLayer* layer = vrv->GetLayer(j);
			switch (layer->IsA())
			{
			case 0://root
				break;
			case 1://view
				break;
			case 2://volume data
				{
					VolumeData* vd = (VolumeData*)layer;
					if (!vd)
						break;
					wxString vname = vd->GetName();
					int mvid = m_data_mgr.GetVolumeIndex(vname);
					if (mvid >= 0) used_vols.at(mvid) = 1;
					//append icon for volume
					m_tree_panel->AppendIcon();
					Color c = vd->GetColor();
					wxColor wxc(
						(unsigned char)(c.r()*255),
						(unsigned char)(c.g()*255),
						(unsigned char)(c.b()*255));
					int ii = m_tree_panel->GetIconNum()-1;
					m_tree_panel->ChangeIconColor(ii, wxc);
					wxTreeItemId item = m_tree_panel->AddVolItem(vrv_item, vd);
					m_tree_panel->SetVolItemImage(item, vd->GetDisp()?2*ii+1:2*ii);
					if (name == vd->GetName() && (type == 2 || type < 0))
					{
						sel_item = item;
						if (set_calc) vrv->SetVolumeA(vd);
						GetBrushToolDlg()->GetSettings(vrv);
						GetMeasureDlg()->GetSettings(vrv);
						GetTraceDlg()->GetSettings(vrv);
					}
				}
				break;
			case 3://mesh data
				{
					MeshData* md = (MeshData*)layer;
					if (!md)
						break;
					//append icon for mesh
					m_tree_panel->AppendIcon();
					Color amb, diff, spec;
					double shine, alpha;
					md->GetMaterial(amb, diff, spec, shine, alpha);
					wxColor wxc(
						(unsigned char)(diff.r()*255),
						(unsigned char)(diff.g()*255),
						(unsigned char)(diff.b()*255));
					int ii = m_tree_panel->GetIconNum()-1;
					m_tree_panel->ChangeIconColor(ii, wxc);
					wxTreeItemId item = m_tree_panel->AddMeshItem(vrv_item, md->GetName());
					m_tree_panel->SetMeshItemImage(item, md->GetDisp()?2*ii+1:2*ii);
					if (name == md->GetName() && (type == 3 || type < 0))
						sel_item = item;//m_tree_panel->SelectItem(item);
				}
				break;
			case 4://annotations
				{
					Annotations* ann = (Annotations*)layer;
					if (!ann)
						break;
					//append icon for annotations
					m_tree_panel->AppendIcon();
					wxColor wxc(255, 255, 255);
					int ii = m_tree_panel->GetIconNum()-1;
					m_tree_panel->ChangeIconColor(ii, wxc);
					wxTreeItemId item = m_tree_panel->AddAnnotationItem(vrv_item, ann->GetName());
					m_tree_panel->SetAnnotationItemImage(item, ann->GetDisp()?2*ii+1:2*ii);
					if (name == ann->GetName() && (type == 4 || type < 0))
						sel_item = item;
				}
				break;
			case 5://group
				{
					DataGroup* group = (DataGroup*)layer;
					if (!group)
						break;
					//append group item to tree
					wxTreeItemId group_item = m_tree_panel->AddGroupItem(vrv_item, group->GetName());
					m_tree_panel->SetGroupItemImage(group_item, int(group->GetDisp()));
					//append volume data to group
					for (k=0; k<group->GetVolumeNum(); k++)
					{
						VolumeData* vd = group->GetVolumeData(k);
						if (!vd)
							continue;
						wxString vname = vd->GetName();
						int mvid = m_data_mgr.GetVolumeIndex(vname);
						if (mvid >= 0) used_vols.at(mvid) = 1;
						//add icon
						m_tree_panel->AppendIcon();
						Color c = vd->GetColor();
						wxColor wxc(
							(unsigned char)(c.r()*255),
							(unsigned char)(c.g()*255),
							(unsigned char)(c.b()*255));
						int ii = m_tree_panel->GetIconNum()-1;
						m_tree_panel->ChangeIconColor(ii, wxc);
						wxTreeItemId item = m_tree_panel->AddVolItem(group_item, vd);
						m_tree_panel->SetVolItemImage(item, vd->GetDisp()?2*ii+1:2*ii);
						if (name == vd->GetName() && (type == 2 || type < 0))
						{
							sel_item = item;
							if (set_calc) vrv->SetVolumeA(vd);
							GetBrushToolDlg()->GetSettings(vrv);
							GetMeasureDlg()->GetSettings(vrv);
							GetTraceDlg()->GetSettings(vrv);
						}
					}
					if (name == group->GetName() && (type == 5 || type < 0))
						sel_item = group_item;
				}
				break;
			case 6://mesh group
				{
					MeshGroup* group = (MeshGroup*)layer;
					if (!group)
						break;
					//append group item to tree
					wxTreeItemId group_item = m_tree_panel->AddMGroupItem(vrv_item, group->GetName());
					m_tree_panel->SetMGroupItemImage(group_item, int(group->GetDisp()));
					//append mesh data to group
					for (k=0; k<group->GetMeshNum(); k++)
					{
						MeshData* md = group->GetMeshData(k);
						if (!md)
							continue;
						//add icon
						m_tree_panel->AppendIcon();
						Color amb, diff, spec;
						double shine, alpha;
						md->GetMaterial(amb, diff, spec, shine, alpha);
						wxColor wxc(
							(unsigned char)(diff.r()*255),
							(unsigned char)(diff.g()*255),
							(unsigned char)(diff.b()*255));
						int ii = m_tree_panel->GetIconNum()-1;
						m_tree_panel->ChangeIconColor(ii, wxc);
						wxTreeItemId item = m_tree_panel->AddMeshItem(group_item, md->GetName());
						m_tree_panel->SetMeshItemImage(item, md->GetDisp()?2*ii+1:2*ii);
						if (name == md->GetName() && (type == 3 || type < 0))
							sel_item = item;
					}
					if (name == group->GetName() && (type == 6 || type < 0))
						sel_item = group_item;
				}
				break;
			}
		}
	}

	for (int j = dm_vnum-1; j >= 0; j--)
	{
		if (used_vols.at(j) == 0)
		{
			VolumeData *testvd = m_data_mgr.GetVolumeData(j);
			if (testvd && testvd->GetDup())
				m_data_mgr.RemoveVolumeData(j);
		}
	}

	m_tree_panel->LoadExpState();
	m_tree_panel->SetScrollPos(wxVERTICAL, scroll_pos);

	if (sel_item.IsOk())
		m_tree_panel->SelectItem(sel_item);

	m_tree_panel->Thaw();
	m_tree_panel->SetEvtHandlerEnabled(true);
}

void VRenderFrame::UpdateROITree(VolumeData *vd, bool set_calc)
{
	if (!vd) return;

	wxTreeItemId item = m_tree_panel->FindTreeItem(vd->GetName());

	if(!item.IsOk()) return;

	m_tree_panel->UpdateVolItem(item, vd);
}


DataManager* VRenderFrame::GetDataManager()
{
	return &m_data_mgr;
}

TreePanel *VRenderFrame::GetTree()
{
	return m_tree_panel;
}

//on selections
void VRenderFrame::OnSelection(int type,
							   VRenderView* vrv,
							   DataGroup* group,
							   VolumeData* vd,
							   MeshData* md,
							   Annotations* ann)
{
	if (m_adjust_view)
	{
		m_adjust_view->SetRenderView(vrv);
		if (!vrv || vd)
			m_adjust_view->SetVolumeData(vd);
	}

	if (m_clip_view)
	{
		switch (type)
		{
		case 2:
			m_clip_view->SetVolumeData(vd);
			break;
		case 3:
			m_clip_view->SetMeshData(md);
			break;
		case 4:
			if (ann)
			{
				VolumeData* vd_ann = ann->GetVolume();
				m_clip_view->SetVolumeData(vd_ann);
			}
			break;
		}
	}

	m_cur_sel_type = type;
	//clear mesh boundbox
	if (m_data_mgr.GetMeshData(m_cur_sel_mesh))
		m_data_mgr.GetMeshData(m_cur_sel_mesh)->SetDrawBounds(false);

	switch (type)
	{
	case 0:  //root
		break;
	case 1:  //view
		if (m_volume_prop)
			m_volume_prop->Show(false);
		if (m_mesh_prop)
			m_mesh_prop->Show(false);
		if (m_mesh_manip)
			m_mesh_manip->Show(false);
		if (m_annotation_prop)
			m_annotation_prop->Show(false);
		m_aui_mgr.GetPane(m_prop_panel).Caption(UITEXT_PROPERTIES);
		m_aui_mgr.Update();
		break;
	case 2:  //volume
		if (vd && vd->GetDisp())
		{
			m_volume_prop->SetVolumeData(vd);
			m_volume_prop->SetGroup(group);
			m_volume_prop->SetView(vrv);
			if (!m_volume_prop->IsShown())
			{
				m_volume_prop->Show(true);
				m_prop_sizer->Clear();
				m_prop_sizer->Add(m_volume_prop, 1, wxEXPAND, 0);
				m_prop_panel->SetSizer(m_prop_sizer);
				m_prop_panel->Layout();
			}
			m_aui_mgr.GetPane(m_prop_panel).Caption(
				wxString(UITEXT_PROPERTIES)+wxString(" - ")+vd->GetName());
			m_aui_mgr.Update();
			wxString str = vd->GetName();
			m_cur_sel_vol = m_data_mgr.GetVolumeIndex(str);

			for (int i=0; i<(int)m_vrv_list.size(); i++)
			{
				VRenderView* vrv = m_vrv_list[i];
				if (!vrv)
					continue;
				vrv->m_glview->m_cur_vol = vd;
			}

			if (m_volume_prop)
				m_volume_prop->Show(true);
			if (m_mesh_prop)
				m_mesh_prop->Show(false);
			if (m_mesh_manip)
				m_mesh_manip->Show(false);
			if (m_annotation_prop)
				m_annotation_prop->Show(false);

			if (m_anno_view)
				m_anno_view->SetData((TreeLayer *)vd);
		}
		else
		{
			if (m_volume_prop)
				m_volume_prop->Show(false);
			if (m_mesh_prop)
				m_mesh_prop->Show(false);
			if (m_mesh_manip)
				m_mesh_manip->Show(false);
			if (m_annotation_prop)
				m_annotation_prop->Show(false);
		}
		break;
	case 3:  //mesh
		if (md)
		{
			m_mesh_prop->SetMeshData(md, vrv);
			if (!m_mesh_prop->IsShown())
			{
				m_mesh_prop->Show(true);
				m_prop_sizer->Clear();
				m_prop_sizer->Add(m_mesh_prop, 1, wxEXPAND, 0);
				m_prop_panel->SetSizer(m_prop_sizer);
				m_prop_panel->Layout();
			}
			m_aui_mgr.GetPane(m_prop_panel).Caption(
				wxString(UITEXT_PROPERTIES)+wxString(" - ")+md->GetName());
			m_aui_mgr.Update();
			wxString str = md->GetName();
			m_cur_sel_mesh = m_data_mgr.GetMeshIndex(str);
			md->SetDrawBounds(true);

			if (m_anno_view)
				m_anno_view->SetData((TreeLayer *)md);
		}

		if (m_volume_prop)
			m_volume_prop->Show(false);
		if (m_mesh_prop && md)
			m_mesh_prop->Show(true);
		if (m_mesh_manip)
			m_mesh_manip->Show(false);
		if (m_annotation_prop)
			m_annotation_prop->Show(false);
		break;
	case 4:  //annotations
		if (ann)
		{
			m_annotation_prop->SetAnnotations(ann, vrv);
			if (!m_annotation_prop->IsShown())
			{
				m_annotation_prop->Show(true);
				m_prop_sizer->Clear();
				m_prop_sizer->Add(m_annotation_prop, 1, wxEXPAND, 0);
				m_prop_panel->SetSizer(m_prop_sizer);
				m_prop_panel->Layout();
			}
			m_aui_mgr.GetPane(m_prop_panel).Caption(
				wxString(UITEXT_PROPERTIES)+wxString(" - ")+ann->GetName());
			m_aui_mgr.Update();
		}

		if (m_volume_prop)
			m_volume_prop->Show(false);
		if (m_mesh_prop)
			m_mesh_prop->Show(false);
		if (m_mesh_manip)
			m_mesh_manip->Show(false);
		if (m_annotation_prop && ann)
			m_annotation_prop->Show(true);
		break;
	case 5:  //group
		if (m_adjust_view)
			m_adjust_view->SetGroup(group);

		if (m_volume_prop)
			m_volume_prop->Show(false);
		if (m_mesh_prop)
			m_mesh_prop->Show(false);
		if (m_mesh_manip)
			m_mesh_manip->Show(false);
		if (m_annotation_prop)
			m_annotation_prop->Show(false);
		m_aui_mgr.GetPane(m_prop_panel).Caption(UITEXT_PROPERTIES);
		m_aui_mgr.Update();
		break;
	case 6:  //mesh manip
		if (md)
		{
			m_mesh_manip->SetMeshData(md);
			m_mesh_manip->GetData();
			if (!m_mesh_manip->IsShown())
			{
				m_mesh_manip->Show(true);
				m_prop_sizer->Clear();
				m_prop_sizer->Add(m_mesh_manip, 1, wxEXPAND, 0);
				m_prop_panel->SetSizer(m_prop_sizer);
				m_prop_panel->Layout();
			}
			m_aui_mgr.GetPane(m_prop_panel).Caption(
				wxString("Manipulations - ")+md->GetName());
			m_aui_mgr.Update();
		}

		if (m_volume_prop)
			m_volume_prop->Show(false);
		if (m_mesh_prop)
			m_mesh_prop->Show(false);
		if (m_mesh_manip && md)
			m_mesh_manip->Show(true);
		if (m_annotation_prop)
			m_annotation_prop->Show(false);
		break;
	default:
		if (m_volume_prop)
			m_volume_prop->Show(false);
		if (m_mesh_prop)
			m_mesh_prop->Show(false);
		if (m_mesh_manip)
			m_mesh_manip->Show(false);
		if (m_annotation_prop)
			m_annotation_prop->Show(false);
		m_aui_mgr.GetPane(m_prop_panel).Caption(UITEXT_PROPERTIES);
		m_aui_mgr.Update();
	}

	if (m_adjust_view && vrv && vrv->GetVolMethod() == VOL_METHOD_MULTI)
		m_adjust_view->SetRenderView(vrv);
}

void VRenderFrame::SetKeyLock(bool lock)
{
	for (int i=0 ; i<(int)m_vrv_list.size() ; i++)
	{
		if (m_vrv_list[i])
			m_vrv_list[i]->SetKeyLock(lock);
	}
}

void VRenderFrame::RefreshVRenderViews(bool tree, bool interactive)
{
	for (int i=0 ; i<(int)m_vrv_list.size() ; i++)
	{
		if (m_vrv_list[i])
			m_vrv_list[i]->RefreshGL(interactive);
	}

	//incase volume color changes
	//change icon color of the tree panel
	if (tree)
		UpdateTreeColors();
}

void VRenderFrame::RefreshVRenderViewsOverlay(bool tree)
{
	for (int i=0 ; i<(int)m_vrv_list.size() ; i++)
	{
		if (m_vrv_list[i])
			m_vrv_list[i]->RefreshGLOverlays();
	}

	//incase volume color changes
	//change icon color of the tree panel
	if (tree)
		UpdateTreeColors();
}

void VRenderFrame::DeleteVRenderView(int i)
{
	if (m_vrv_list[i])
	{
		int j;
		wxString str = m_vrv_list[i]->GetName();

		for (j=0 ; j<m_vrv_list[i]->GetAllVolumeNum() ; j++)
			m_vrv_list[i]->GetAllVolumeData(j)->SetDisp(true);
		for (j=0 ; j<m_vrv_list[i]->GetMeshNum() ; j++)
			m_vrv_list[i]->GetMeshData(j)->SetDisp(true);
		VRenderView* vrv = m_vrv_list[i];
		m_vrv_list.erase(m_vrv_list.begin()+i);
		m_aui_mgr.DetachPane(vrv);
		vrv->Close();
		delete vrv;
		m_aui_mgr.Update();
		UpdateTree();

		if (m_movie_view)
			m_movie_view->DeleteView(str);
	}
}

void VRenderFrame::DeleteVRenderView(wxString &name)
{
	for (int i=0; i<GetViewNum(); i++)
	{
		VRenderView* vrv = GetView(i);
		if (vrv && name == vrv->GetName())
		{
			DeleteVRenderView(i);
			return;
		}
	}
}

AdjustView* VRenderFrame::GetAdjustView()
{
	return m_adjust_view;
}

//organize render views
void VRenderFrame::OrganizeVRenderViews(int mode)
{
	int width = 800;
	int height = 600;
	int paneNum = (int)m_vrv_list.size();
	int i;
	for (i=0; i<paneNum; i++)
	{
		wxAuiPaneInfo auiInfo;
		VRenderView* vrv = m_vrv_list[i];
		if (vrv)
			auiInfo = m_aui_mgr.GetPane(vrv);

		if (auiInfo.IsOk())
		{
			switch (mode)
			{
			case 0://top-bottom
				auiInfo.MinSize(width, height/paneNum);
				break;
			case 1://left-right
				auiInfo.MinSize(width/paneNum, height);
				break;
			}
		}
	}
	m_aui_mgr.Update();
}

//hide/show tools
void VRenderFrame::ToggleAllTools()
{
	if (m_aui_mgr.GetPane(m_tree_panel).IsShown() ||
		m_aui_mgr.GetPane(m_measure_dlg).IsShown() ||
		m_aui_mgr.GetPane(m_movie_view).IsShown() ||
		m_aui_mgr.GetPane(m_prop_panel).IsShown() ||
		m_aui_mgr.GetPane(m_adjust_view).IsShown() ||
		m_aui_mgr.GetPane(m_clip_view).IsShown())
		m_ui_state = true;
	else if (!m_aui_mgr.GetPane(m_tree_panel).IsShown() &&
		!m_aui_mgr.GetPane(m_measure_dlg).IsShown() &&
		!m_aui_mgr.GetPane(m_movie_view).IsShown() &&
		!m_aui_mgr.GetPane(m_prop_panel).IsShown() &&
		!m_aui_mgr.GetPane(m_adjust_view).IsShown() &&
		!m_aui_mgr.GetPane(m_clip_view).IsShown())
		m_ui_state = false;

	if (m_ui_state)
	{
		if (!m_ui_state_cache.empty()) m_ui_state_cache.clear();
		m_ui_state_cache[m_tree_panel->GetName()]  = m_aui_mgr.GetPane(m_tree_panel).IsShown();
		m_ui_state_cache[m_movie_view->GetName()]  = m_aui_mgr.GetPane(m_movie_view).IsShown();
		m_ui_state_cache[m_measure_dlg->GetName()] = m_aui_mgr.GetPane(m_measure_dlg).IsShown();
		m_ui_state_cache[m_prop_panel->GetName()]  = m_aui_mgr.GetPane(m_prop_panel).IsShown();
		m_ui_state_cache[m_adjust_view->GetName()] = m_aui_mgr.GetPane(m_adjust_view).IsShown();
		m_ui_state_cache[m_clip_view->GetName()]   = m_aui_mgr.GetPane(m_clip_view).IsShown();

		//hide all
		//scene view
		m_aui_mgr.GetPane(m_tree_panel).Hide();
		m_tb_menu_ui->Check(ID_UITreeView, false);
		//movie view 
		m_aui_mgr.GetPane(m_movie_view).Hide();
		m_tb_menu_ui->Check(ID_UIMovieView, false);
		//measurement
		m_aui_mgr.GetPane(m_measure_dlg).Hide();
		m_tb_menu_ui->Check(ID_UIMeasureView, false);
		//properties
		m_aui_mgr.GetPane(m_prop_panel).Hide();
		m_tb_menu_ui->Check(ID_UIPropView, false);
		//adjust view
		m_aui_mgr.GetPane(m_adjust_view).Hide();
		m_tb_menu_ui->Check(ID_UIAdjView, false);
		//clipping view
		m_aui_mgr.GetPane(m_clip_view).Hide();
		m_tb_menu_ui->Check(ID_UIClipView, false);

		m_ui_state = false;
	}
	else
	{
		if (m_ui_state_cache.empty())
		{
			//show all
			//scene view
			m_aui_mgr.GetPane(m_tree_panel).Show();
			m_tb_menu_ui->Check(ID_UITreeView, true);
			//movie view 
			m_aui_mgr.GetPane(m_movie_view).Show();
			m_tb_menu_ui->Check(ID_UIMovieView, true);
			//measurement
			m_aui_mgr.GetPane(m_measure_dlg).Show();
			m_tb_menu_ui->Check(ID_UIMeasureView, true);
			//properties
			m_aui_mgr.GetPane(m_prop_panel).Show();
			m_tb_menu_ui->Check(ID_UIPropView, true);
			//adjust view
			m_aui_mgr.GetPane(m_adjust_view).Show();
			m_tb_menu_ui->Check(ID_UIAdjView, true);
			//clipping view
			m_aui_mgr.GetPane(m_clip_view).Show();
			m_tb_menu_ui->Check(ID_UIClipView, true);
		}
		else
		{
			//scene view
			if (m_ui_state_cache[m_tree_panel->GetName()])
			{
				m_aui_mgr.GetPane(m_tree_panel).Show();
				m_tb_menu_ui->Check(ID_UITreeView, true);
			}
			//movie view 
			if (m_ui_state_cache[m_movie_view->GetName()])
			{
				m_aui_mgr.GetPane(m_movie_view).Show();
				m_tb_menu_ui->Check(ID_UIMovieView, true);
			}
			//measurement
			if (m_ui_state_cache[m_measure_dlg->GetName()])
			{
				m_aui_mgr.GetPane(m_measure_dlg).Show();
				m_tb_menu_ui->Check(ID_UIMeasureView, true);
			}
			//properties
			if (m_ui_state_cache[m_prop_panel->GetName()])
			{
				m_aui_mgr.GetPane(m_prop_panel).Show();
				m_tb_menu_ui->Check(ID_UIPropView, true);
			}
			//adjust view
			if (m_ui_state_cache[m_adjust_view->GetName()])
			{
				m_aui_mgr.GetPane(m_adjust_view).Show();
				m_tb_menu_ui->Check(ID_UIAdjView, true);
			}
			//clipping view
			if (m_ui_state_cache[m_clip_view->GetName()])
			{
				m_aui_mgr.GetPane(m_clip_view).Show();
				m_tb_menu_ui->Check(ID_UIClipView, true);
			}
		}

		m_ui_state = true;
	}

	m_aui_mgr.Update();
}

void VRenderFrame::ShowPane(wxPanel* pane, bool show)
{
	if (m_aui_mgr.GetPane(pane).IsOk())
	{
		if (show)
			m_aui_mgr.GetPane(pane).Show();
		else
			m_aui_mgr.GetPane(pane).Hide();
		m_aui_mgr.Update();
	}
}

void VRenderFrame::OnChEmbedCheck(wxCommandEvent &event)
{
	wxCheckBox* ch_embed = (wxCheckBox*)event.GetEventObject();
	if (ch_embed)
		m_vrp_embed = ch_embed->GetValue();
}

void VRenderFrame::OnChSaveCmpCheck(wxCommandEvent &event)
{
	wxCheckBox* ch_cmp = (wxCheckBox*)event.GetEventObject();
	if (ch_cmp)
		m_save_compress = ch_cmp->GetValue();
}

wxWindow* VRenderFrame::CreateExtraControlProjectSave(wxWindow* parent)
{
	wxPanel* panel = new wxPanel(parent, 0, wxDefaultPosition, wxSize(600, 90));

	wxBoxSizer *group1 = new wxStaticBoxSizer(
		new wxStaticBox(panel, wxID_ANY, "Additional Options"), wxVERTICAL );

	//copy all files check box
	wxCheckBox* ch_embed = new wxCheckBox(panel, wxID_HIGHEST+3005,
		"Embed all files in the project folder");
	ch_embed->Connect(ch_embed->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(VRenderFrame::OnChEmbedCheck), NULL, panel);
	ch_embed->SetValue(m_vrp_embed);

	//compressed
	wxCheckBox* ch_cmp = new wxCheckBox(panel, wxID_HIGHEST+3004,
		"Compression");
	ch_cmp->Connect(ch_cmp->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(VRenderFrame::OnChSaveCmpCheck), NULL, panel);
	if (ch_cmp)
		ch_cmp->SetValue(m_save_compress);

	//group
	group1->Add(10, 10);
	group1->Add(ch_embed);
	group1->Add(10, 10);
	group1->Add(ch_cmp);
	group1->Add(10, 10);

	panel->SetSizer(group1);
	panel->Layout();

	return panel;
}

void VRenderFrame::OnSaveProject(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog *fopendlg = new wxFileDialog(
		this, "Save Project File",
		"", "", "*.vrp", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
	fopendlg->SetExtraControlCreator(CreateExtraControlProjectSave);

	int rval = fopendlg->ShowModal();
	if (rval == wxID_OK)
	{
		wxString filename = fopendlg->GetPath();
		SaveProject(filename);
	}

	delete fopendlg;
}

void VRenderFrame::OnOpenProject(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog *fopendlg = new wxFileDialog(
		this, "Choose Project File",
		"", "", "*.vrp", wxFD_OPEN);

	int rval = fopendlg->ShowModal();
	if (rval == wxID_OK)
	{
		wxString path = fopendlg->GetPath();
		OpenProject(path);
	}

	delete fopendlg;
}

void VRenderFrame::SaveProject(wxString& filename)
{
	wxFileConfig fconfig("FluoRender Project");

	int i, j, k;
	fconfig.Write("ver_major", VERSION_MAJOR_TAG);
	fconfig.Write("ver_minor", VERSION_MINOR_TAG);

	int ticks = m_data_mgr.GetVolumeNum() + m_data_mgr.GetMeshNum();
	int tick_cnt = 1;
	fconfig.Write("ticks", ticks);
	wxProgressDialog *prg_diag = 0;
	prg_diag = new wxProgressDialog(
		"FluoRender: Saving project...",
		"Saving project file. Please wait.",
		100, 0, wxPD_SMOOTH|wxPD_ELAPSED_TIME|wxPD_AUTO_HIDE);

	wxString str;

	//save streaming mode
	fconfig.SetPath("/memory settings");
	fconfig.Write("mem swap", m_setting_dlg->GetMemSwap());
	fconfig.Write("graphics mem", m_setting_dlg->GetGraphicsMem());
	fconfig.Write("large data size", m_setting_dlg->GetLargeDataSize());
	fconfig.Write("force brick size", m_setting_dlg->GetForceBrickSize());
	fconfig.Write("up time", m_setting_dlg->GetResponseTime());
	fconfig.Write("update order", m_setting_dlg->GetUpdateOrder());

	//save data list
	//volume
	fconfig.SetPath("/data/volume");
	fconfig.Write("num", m_data_mgr.GetVolumeNum());
	for (i=0; i<m_data_mgr.GetVolumeNum(); i++)
	{
		if (ticks && prg_diag)
			prg_diag->Update(90*tick_cnt/ticks,
			"Saving volume data. Please wait.");
		tick_cnt++;

		VolumeData* vd = m_data_mgr.GetVolumeData(i);
		if (vd)
		{
			str = wxString::Format("/data/volume/%d", i);
			//name
			fconfig.SetPath(str);
			str = vd->GetName();
			fconfig.Write("name", str);
			//compression
			fconfig.Write("compression", m_compression);
			//skip brick
			fconfig.Write("skip_brick", vd->GetSkipBrick());
			//path
			str = vd->GetPath();
			if (str == "" || m_vrp_embed)
			{
				wxString new_folder;
				new_folder = filename + "_files";
				CREATE_DIR(new_folder.fn_str());
				str = new_folder + GETSLASH() + vd->GetName() + ".nrrd";
				vd->Save(str, 2, false, VRenderFrame::GetCompression());
				fconfig.Write("path", str);
			}
			else
				fconfig.Write("path", str);

			if (vd->GetReader())
			{
				fconfig.Write("slice_seq", vd->GetReader()->GetSliceSeq());
				str = vd->GetReader()->GetTimeId();
				fconfig.Write("time_id", str);
			}
			else
			{
				fconfig.Write("slice_seq", false);
				fconfig.Write("time_id", "");
			}
			fconfig.Write("cur_time", vd->GetCurTime());
			fconfig.Write("cur_chan", m_vrp_embed?0:vd->GetCurChannel());

			//volume properties
			fconfig.SetPath("properties");
			fconfig.Write("display", vd->GetDisp());

			//properties
			fconfig.Write("3dgamma", vd->Get3DGamma());
			fconfig.Write("boundary", vd->GetBoundary());
			fconfig.Write("contrast", vd->GetOffset());
			fconfig.Write("left_thresh", vd->GetLeftThresh());
			fconfig.Write("right_thresh", vd->GetRightThresh());
			Color color = vd->GetColor();
			str = wxString::Format("%f %f %f", color.r(), color.g(), color.b());
			fconfig.Write("color", str);
			double hue, sat, val;
			vd->GetHSV(hue, sat, val);
			str = wxString::Format("%f %f %f", hue, sat, val);
			fconfig.Write("hsv", str);
			color = vd->GetMaskColor();
			str = wxString::Format("%f %f %f", color.r(), color.g(), color.b());
			fconfig.Write("mask_color", str);
			fconfig.Write("mask_color_set", vd->GetMaskColorSet());
			fconfig.Write("enable_alpha", vd->GetEnableAlpha());
			fconfig.Write("alpha", vd->GetAlpha());
			double amb, diff, spec, shine;
			vd->GetMaterial(amb, diff, spec, shine);
			fconfig.Write("ambient", amb);
			fconfig.Write("diffuse", diff);
			fconfig.Write("specular", spec);
			fconfig.Write("shininess", shine);
			fconfig.Write("shading", vd->GetShading());
			fconfig.Write("samplerate", vd->GetSampleRate());

			//resolution scale
			double resx, resy, resz;
			double b_resx, b_resy, b_resz;
			double s_resx, s_resy, s_resz;
			double sclx, scly, sclz;
			vd->GetSpacings(resx, resy, resz);
			vd->GetBaseSpacings(b_resx, b_resy, b_resz);
			vd->GetSpacingScales(s_resx, s_resy, s_resz);
			vd->GetScalings(sclx, scly, sclz);
			str = wxString::Format("%lf %lf %lf", resx, resy, resz);
			fconfig.Write("res", str);
			str = wxString::Format("%lf %lf %lf", b_resx, b_resy, b_resz);
			fconfig.Write("b_res", str);
			str = wxString::Format("%lf %lf %lf", s_resx, s_resy, s_resz);
			fconfig.Write("s_res", str);
			str = wxString::Format("%lf %lf %lf", sclx, scly, sclz);
			fconfig.Write("scl", str);

			//planes
			vector<Plane*> *planes = 0;
			if (vd->GetVR())
				planes = vd->GetVR()->get_planes();
			if (planes && planes->size() == 6)
			{
				Plane* plane = 0;
				double abcd[4];

				//x1
				plane = (*planes)[0];
				plane->get_copy(abcd);
				fconfig.Write("x1_val", abcd[3]);
				//x2
				plane = (*planes)[1];
				plane->get_copy(abcd);
				fconfig.Write("x2_val", abcd[3]);
				//y1
				plane = (*planes)[2];
				plane->get_copy(abcd);
				fconfig.Write("y1_val", abcd[3]);
				//y2
				plane = (*planes)[3];
				plane->get_copy(abcd);
				fconfig.Write("y2_val", abcd[3]);
				//z1
				plane = (*planes)[4];
				plane->get_copy(abcd);
				fconfig.Write("z1_val", abcd[3]);
				//z2
				plane = (*planes)[5];
				plane->get_copy(abcd);
				fconfig.Write("z2_val", abcd[3]);
			}

			//2d adjustment settings
			str = wxString::Format("%f %f %f", vd->GetGamma().r(), vd->GetGamma().g(), vd->GetGamma().b());
			fconfig.Write("gamma", str);
			str = wxString::Format("%f %f %f", vd->GetBrightness().r(), vd->GetBrightness().g(), vd->GetBrightness().b());
			fconfig.Write("brightness", str);
			str = wxString::Format("%f %f %f", vd->GetHdr().r(), vd->GetHdr().g(), vd->GetHdr().b());
			fconfig.Write("hdr", str);
			fconfig.Write("sync_r", vd->GetSyncR());
			fconfig.Write("sync_g", vd->GetSyncG());
			fconfig.Write("sync_b", vd->GetSyncB());

			//colormap settings
			fconfig.Write("colormap_mode", vd->GetColormapMode());
			fconfig.Write("colormap", vd->GetColormap());
			double low, high;
			vd->GetColormapValues(low, high);
			fconfig.Write("colormap_lo_value", low);
			fconfig.Write("colormap_hi_value", high);
			fconfig.Write("id_color_disp_mode", vd->GetIDColDispMode());

			//inversion
			fconfig.Write("inv", vd->GetInvert());
			//mip enable
			fconfig.Write("mode", vd->GetMode());
			//noise reduction
			fconfig.Write("noise_red", vd->GetNR());
			//depth override
			fconfig.Write("depth_ovrd", vd->GetBlendMode());

			//shadow
			fconfig.Write("shadow", vd->GetShadow());
			//shadow intensity
			double shadow_int;
			vd->GetShadowParams(shadow_int);
			fconfig.Write("shadow_darkness", shadow_int);

			//legend
			fconfig.Write("legend", vd->GetLegend());

			//roi
			wstring tree_str = vd->ExportROITree();
			if (!tree_str.empty())
			{
				wxString wxstr(tree_str);
				fconfig.Write("roi_tree", wxstr);
			}
			string roi_ids_str = vd->ExportSelIDs();
			if (!roi_ids_str.empty())
			{
				wxString wxstr(roi_ids_str);
				fconfig.Write("selected_rois", wxstr);
			}
			fconfig.Write("roi_disp_mode", vd->GetIDColDispMode());

			//mask
			Nrrd* mask = vd->GetMask(true);
			str = "";
			if (mask)
			{
				wxString new_folder;
				new_folder = filename + "_files";
				CREATE_DIR(new_folder.fn_str());
				str = new_folder + GETSLASH() + vd->GetName() + ".msk";
				MSKWriter msk_writer;
				msk_writer.SetData(mask);
				msk_writer.SetSpacings(resx, resy, resz);
				msk_writer.Save(str.ToStdWstring(), 0);
			}
			fconfig.Write("mask", str);
		}
	}
	//mesh
	fconfig.SetPath("/data/mesh");
	fconfig.Write("num", m_data_mgr.GetMeshNum());
	for (i=0; i<m_data_mgr.GetMeshNum(); i++)
	{
		if (ticks && prg_diag)
			prg_diag->Update(90*tick_cnt/ticks,
			"Saving mesh data. Please wait.");
		tick_cnt++;

		MeshData* md = m_data_mgr.GetMeshData(i);
		if (md)
		{
			if (md->GetPath() == "" || m_vrp_embed)
			{
				wxString new_folder;
				new_folder = filename + "_files";
				CREATE_DIR(new_folder.fn_str());
				str = new_folder + GETSLASH() + md->GetName() + ".obj";
				md->Save(str);
			}
			str = wxString::Format("/data/mesh/%d", i);
			fconfig.SetPath(str);
			str = md->GetName();
			fconfig.Write("name", str);
			str = md->GetPath();
			fconfig.Write("path", str);
			//mesh prperties
			fconfig.SetPath("properties");
			fconfig.Write("display", md->GetDisp());
			//lighting
			fconfig.Write("lighting", md->GetLighting());
			//material
			Color amb, diff, spec;
			double shine, alpha;
			md->GetMaterial(amb, diff, spec, shine, alpha);
			str = wxString::Format("%f %f %f", amb.r(), amb.g(), amb.b());
			fconfig.Write("ambient", str);
			str = wxString::Format("%f %f %f", diff.r(), diff.g(), diff.b());
			fconfig.Write("diffuse", str);
			str = wxString::Format("%f %f %f", spec.r(), spec.g(), spec.b());
			fconfig.Write("specular", str);
			fconfig.Write("shininess", shine);
			fconfig.Write("alpha", alpha);
			//2d adjustment settings
			str = wxString::Format("%f %f %f", md->GetGamma().r(), md->GetGamma().g(), md->GetGamma().b());
			fconfig.Write("gamma", str);
			str = wxString::Format("%f %f %f", md->GetBrightness().r(), md->GetBrightness().g(), md->GetBrightness().b());
			fconfig.Write("brightness", str);
			str = wxString::Format("%f %f %f", md->GetHdr().r(), md->GetHdr().g(), md->GetHdr().b());
			fconfig.Write("hdr", str);
			fconfig.Write("sync_r", md->GetSyncR());
			fconfig.Write("sync_g", md->GetSyncG());
			fconfig.Write("sync_b", md->GetSyncB());
			//shadow
			fconfig.Write("shadow", md->GetShadow());
			double darkness;
			md->GetShadowParams(darkness);
			fconfig.Write("shadow_darkness", darkness);

			//mesh transform
			fconfig.SetPath("../transform");
			double x, y, z;
			md->GetTranslation(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("translation", str);
			md->GetRotation(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("rotation", str);
			md->GetScaling(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("scaling", str);
		}
	}
	//annotations
	fconfig.SetPath("/data/annotations");
	fconfig.Write("num", m_data_mgr.GetAnnotationNum());
	for (i=0; i<m_data_mgr.GetAnnotationNum(); i++)
	{
		Annotations* ann = m_data_mgr.GetAnnotations(i);
		if (ann)
		{
			if (ann->GetPath() == "")
			{
				wxString new_folder;
				new_folder = filename + "_files";
				CREATE_DIR(new_folder.fn_str());
				str = new_folder + GETSLASH() + ann->GetName() + ".txt";
				ann->Save(str);
			}
			str = wxString::Format("/data/annotations/%d", i);
			fconfig.SetPath(str);
			str = ann->GetName();
			fconfig.Write("name", str);
			str = ann->GetPath();
			fconfig.Write("path", str);
		}
	}
	//views
	fconfig.SetPath("/views");
	fconfig.Write("num", (int)m_vrv_list.size());
	for (i=0; i<(int)m_vrv_list.size(); i++)
	{
		VRenderView* vrv = m_vrv_list[i];
		if (vrv)
		{
			str = wxString::Format("/views/%d", i);
			fconfig.SetPath(str);
			//view layers
			str = wxString::Format("/views/%d/layers", i);
			fconfig.SetPath(str);
			fconfig.Write("num", vrv->GetLayerNum());
			for (j=0; j<vrv->GetLayerNum(); j++)
			{
				TreeLayer* layer = vrv->GetLayer(j);
				if (!layer)
					continue;
				str = wxString::Format("/views/%d/layers/%d", i, j);
				fconfig.SetPath(str);
				switch (layer->IsA())
				{
				case 2://volume data
					fconfig.Write("type", 2);
					fconfig.Write("name", layer->GetName());
					break;
				case 3://mesh data
					fconfig.Write("type", 3);
					fconfig.Write("name", layer->GetName());
					break;
				case 4://annotations
					fconfig.Write("type", 4);
					fconfig.Write("name", layer->GetName());
					break;
				case 5://group
					{
						DataGroup* group = (DataGroup*)layer;

						fconfig.Write("type", 5);
						fconfig.Write("name", layer->GetName());
						fconfig.Write("id", DataGroup::GetID());
						//dispaly
						fconfig.Write("display", group->GetDisp());
						//2d adjustment
						str = wxString::Format("%f %f %f", group->GetGamma().r(),
							group->GetGamma().g(), group->GetGamma().b());
						fconfig.Write("gamma", str);
						str = wxString::Format("%f %f %f", group->GetBrightness().r(),
							group->GetBrightness().g(), group->GetBrightness().b());
						fconfig.Write("brightness", str);
						str = wxString::Format("%f %f %f", group->GetHdr().r(),
							group->GetHdr().g(), group->GetHdr().b());
						fconfig.Write("hdr", str);
						fconfig.Write("sync_r", group->GetSyncR());
						fconfig.Write("sync_g", group->GetSyncG());
						fconfig.Write("sync_b", group->GetSyncB());
						//sync volume properties
						fconfig.Write("sync_vp", group->GetVolumeSyncProp());
						//sync volume spacings
						fconfig.Write("sync_vspc", group->GetVolumeSyncSpc());
						//volumes
						str = wxString::Format("/views/%d/layers/%d/volumes", i, j);
						fconfig.SetPath(str);
						fconfig.Write("num", group->GetVolumeNum());
						for (k=0; k<group->GetVolumeNum(); k++)
							fconfig.Write(wxString::Format("vol_%d", k), group->GetVolumeData(k)->GetName());

					}
					break;
				case 6://mesh group
					{
						MeshGroup* group = (MeshGroup*)layer;

						fconfig.Write("type", 6);
						fconfig.Write("name", layer->GetName());
						fconfig.Write("id", MeshGroup::GetID());
						//display
						fconfig.Write("display", group->GetDisp());
						//sync mesh properties
						fconfig.Write("sync_mp", group->GetMeshSyncProp());
						//meshes
						str = wxString::Format("/views/%d/layers/%d/meshes", i, j);
						fconfig.SetPath(str);
						fconfig.Write("num", group->GetMeshNum());
						for (k=0; k<group->GetMeshNum(); k++)
							fconfig.Write(wxString::Format("mesh_%d", k), group->GetMeshData(k)->GetName());
					}
					break;
				}
			}

			//properties
			fconfig.SetPath(wxString::Format("/views/%d/properties", i));
			fconfig.Write("drawall", vrv->GetDraw());
			fconfig.Write("persp", vrv->GetPersp());
			fconfig.Write("free", vrv->GetFree());
			fconfig.Write("aov", vrv->GetAov());
			fconfig.Write("min_dpi", vrv->GetMinPPI());
			fconfig.Write("res_mode", vrv->GetResMode());
			fconfig.Write("nearclip", vrv->GetNearClip());
			fconfig.Write("farclip", vrv->GetFarClip());
			Color bkcolor;
			bkcolor = vrv->GetBackgroundColor();
			str = wxString::Format("%f %f %f", bkcolor.r(), bkcolor.g(), bkcolor.b());
			fconfig.Write("backgroundcolor", str);
			fconfig.Write("drawtype", vrv->GetDrawType());
			fconfig.Write("volmethod", vrv->GetVolMethod());
			fconfig.Write("peellayers", vrv->GetPeelingLayers());
			fconfig.Write("fog", vrv->GetFog());
			fconfig.Write("fogintensity", (double)vrv->GetFogIntensity());
			fconfig.Write("draw_camctr", vrv->m_glview->m_draw_camctr);
			fconfig.Write("draw_fps", vrv->m_glview->m_draw_info);
			fconfig.Write("draw_legend", vrv->m_glview->m_draw_legend);

			double x, y, z;
			//camera
			vrv->GetTranslations(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("translation", str);
			vrv->GetRotations(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("rotation", str);
			vrv->GetCenters(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("center", str);
			fconfig.Write("centereyedist", vrv->GetCenterEyeDist());
			fconfig.Write("radius", vrv->GetRadius());
			fconfig.Write("initdist", vrv->m_glview->GetInitDist());
			fconfig.Write("scale", vrv->m_glview->m_scale_factor);
			//object
			vrv->GetObjCenters(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("obj_center", str);
			vrv->GetObjTrans(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("obj_trans", str);
			vrv->GetObjRot(x, y, z);
			str = wxString::Format("%f %f %f", x, y, z);
			fconfig.Write("obj_rot", str);
			//scale bar
			fconfig.Write("disp_scale_bar", vrv->m_glview->m_disp_scale_bar);
			fconfig.Write("disp_scale_bar_text", vrv->m_glview->m_disp_scale_bar_text);
			fconfig.Write("sb_length", vrv->m_glview->m_sb_length);
			str = vrv->m_glview->m_sb_text;
			fconfig.Write("sb_text", str);
			str = vrv->m_glview->m_sb_num;
			fconfig.Write("sb_num", str);
			fconfig.Write("sb_unit", vrv->m_glview->m_sb_unit);

			//2d adjustment
			str = wxString::Format("%f %f %f", vrv->m_glview->GetGamma().r(),
				vrv->m_glview->GetGamma().g(), vrv->m_glview->GetGamma().b());
			fconfig.Write("gamma", str);
			str = wxString::Format("%f %f %f", vrv->m_glview->GetBrightness().r(),
				vrv->m_glview->GetBrightness().g(), vrv->m_glview->GetBrightness().b());
			fconfig.Write("brightness", str);
			str = wxString::Format("%f %f %f", vrv->m_glview->GetHdr().r(),
				vrv->m_glview->GetHdr().g(), vrv->m_glview->GetHdr().b());
			fconfig.Write("hdr", str);
			fconfig.Write("sync_r", vrv->m_glview->GetSyncR());
			fconfig.Write("sync_g", vrv->m_glview->GetSyncG());
			fconfig.Write("sync_b", vrv->m_glview->GetSyncB());

			//clipping plane rotations
			fconfig.Write("clip_mode", vrv->GetClipMode());
			double rotx_cl, roty_cl, rotz_cl;
			vrv->GetClippingPlaneRotations(rotx_cl, roty_cl, rotz_cl);
			fconfig.Write("rotx_cl", rotx_cl);
			fconfig.Write("roty_cl", roty_cl);
			fconfig.Write("rotz_cl", rotz_cl);

			//painting parameters
			fconfig.Write("brush_use_pres", vrv->GetBrushUsePres());
			fconfig.Write("brush_size_1", vrv->GetBrushSize1());
			fconfig.Write("brush_size_2", vrv->GetBrushSize2());
			fconfig.Write("brush_spacing", vrv->GetBrushSpacing());
			fconfig.Write("brush_iteration", vrv->GetBrushIteration());
			fconfig.Write("brush_translate", vrv->GetBrushSclTranslate());
			fconfig.Write("w2d", vrv->GetW2d());

			//rulers
			fconfig.SetPath(wxString::Format("/views/%d/rulers", i));
			vector<Ruler*>* ruler_list = vrv->GetRulerList();
			if (ruler_list && ruler_list->size())
			{
				fconfig.Write("num", static_cast<unsigned int>(ruler_list->size()));
				for (size_t ri=0; ri<ruler_list->size(); ++ri)
				{
					Ruler* ruler = (*ruler_list)[ri];
					if (!ruler) continue;
					fconfig.SetPath(wxString::Format("/views/%d/rulers/%d", i, (int)ri));
					fconfig.Write("name", ruler->GetName());
					fconfig.Write("name_disp", ruler->GetNameDisp());
					fconfig.Write("description", ruler->GetDesc());
					fconfig.Write("type", ruler->GetRulerType());
					fconfig.Write("display", ruler->GetDisp());
					fconfig.Write("transient", ruler->GetTimeDep());
					fconfig.Write("time", ruler->GetTime());
					fconfig.Write("info_names", ruler->GetInfoNames());
					fconfig.Write("info_values", ruler->GetInfoValues());
					fconfig.Write("use_color", ruler->GetUseColor());
					fconfig.Write("color", wxString::Format("%f %f %f",
						ruler->GetColor().r(), ruler->GetColor().g(), ruler->GetColor().b()));
					fconfig.Write("num", ruler->GetNumPoint());
					for (size_t rpi=0; rpi<ruler->GetNumPoint(); ++rpi)
					{
						Point* rp = ruler->GetPoint(rpi);
						if (!rp) continue;
						fconfig.SetPath(wxString::Format("/views/%d/rulers/%d/points/%d", i, (int)ri, (int)rpi));
						fconfig.Write("point", wxString::Format("%f %f %f", rp->x(), rp->y(), rp->z()));
					}
				}
			}
		}
	}
	//clipping planes
	fconfig.SetPath("/prop_panel");
	fconfig.Write("cur_sel_type", m_cur_sel_type);
	fconfig.Write("cur_sel_vol", m_cur_sel_vol);
	fconfig.Write("cur_sel_mesh", m_cur_sel_mesh);
	fconfig.Write("chann_link", m_clip_view->GetChannLink());
	fconfig.Write("plane_mode", m_clip_view->GetPlaneMode());
	fconfig.Write("x_link", m_clip_view->GetXLink());
	fconfig.Write("y_link", m_clip_view->GetYLink());
	fconfig.Write("z_link", m_clip_view->GetZLink());
	//movie view
	fconfig.SetPath("/movie_panel");
	fconfig.Write("views_cmb", m_movie_view->m_views_cmb->GetCurrentSelection());
	fconfig.Write("rot_check", m_movie_view->m_rot_chk->GetValue());
	fconfig.Write("seq_check", m_movie_view->m_seq_chk->GetValue());
	fconfig.Write("frame_range", m_movie_view->m_progress_sldr->GetMax());
	fconfig.Write("time_frame", m_movie_view->m_progress_sldr->GetValue());
	fconfig.Write("x_rd", m_movie_view->m_x_rd->GetValue());
	fconfig.Write("y_rd", m_movie_view->m_y_rd->GetValue());
	fconfig.Write("z_rd", m_movie_view->m_z_rd->GetValue());
	fconfig.Write("angle_end_text", m_movie_view->m_degree_end->GetValue());
	fconfig.Write("step_text", m_movie_view->m_movie_time->GetValue());
	fconfig.Write("frames_text", m_movie_view->m_fps_text->GetValue());
	fconfig.Write("frame_chk", m_movie_view->m_frame_chk->GetValue());
	fconfig.Write("center_x_text", m_movie_view->m_center_x_text->GetValue());
	fconfig.Write("center_y_text", m_movie_view->m_center_y_text->GetValue());
	fconfig.Write("width_text", m_movie_view->m_width_text->GetValue());
	fconfig.Write("height_text", m_movie_view->m_height_text->GetValue());
	fconfig.Write("time_start_text", m_movie_view->m_time_start_text->GetValue());
	fconfig.Write("time_end_text", m_movie_view->m_time_end_text->GetValue());
	//brushtool diag
	fconfig.SetPath("/brush_diag");
	fconfig.Write("ca_min", m_brush_tool_dlg->GetDftCAMin());
	fconfig.Write("ca_max", m_brush_tool_dlg->GetDftCAMax());
	fconfig.Write("ca_thresh", m_brush_tool_dlg->GetDftCAThresh());
	fconfig.Write("nr_thresh", m_brush_tool_dlg->GetDftNRThresh());
	fconfig.Write("nr_size", m_brush_tool_dlg->GetDftNRSize());
	//tree_panel
	fconfig.SetPath("/tree_panel");
	wxString item_states = m_tree_panel->ExportExpState();
	fconfig.Write("exp_states", item_states);
	//ui layout
	fconfig.SetPath("/ui_layout");
	fconfig.Write("ui_main_tb", m_main_tb->IsShown());
	fconfig.Write("ui_tree_view", m_tree_panel->IsShown());
	fconfig.Write("ui_measure_view", m_measure_dlg->IsShown());
	fconfig.Write("ui_adjust_view", m_adjust_view->IsShown());
	fconfig.Write("ui_clip_view", m_clip_view->IsShown());
	fconfig.Write("ui_prop_view", m_prop_panel->IsShown());
	fconfig.Write("ui_main_tb_float", m_aui_mgr.GetPane(m_main_tb).IsOk()?
		m_aui_mgr.GetPane(m_main_tb).IsFloating():false);
	fconfig.Write("ui_tree_view_float", m_aui_mgr.GetPane(m_tree_panel).IsOk()?
		m_aui_mgr.GetPane(m_tree_panel).IsFloating():false);
	fconfig.Write("ui_measure_view_float", m_aui_mgr.GetPane(m_measure_dlg).IsOk()?
		m_aui_mgr.GetPane(m_measure_dlg).IsFloating():false);
	fconfig.Write("ui_adjust_view_float", m_aui_mgr.GetPane(m_adjust_view).IsOk()?
		m_aui_mgr.GetPane(m_adjust_view).IsFloating():false);
	fconfig.Write("ui_clip_view_float", m_aui_mgr.GetPane(m_clip_view).IsOk()?
		m_aui_mgr.GetPane(m_clip_view).IsFloating():false);
	fconfig.Write("ui_prop_view_float", m_aui_mgr.GetPane(m_prop_panel).IsOk()?
		m_aui_mgr.GetPane(m_prop_panel).IsFloating():false);
	//interpolator
	fconfig.SetPath("/interpolator");
	fconfig.Write("max_id", Interpolator::m_id);
	int group_num = m_interpolator.GetKeyNum();
	fconfig.Write("num", group_num);
	for (i=0; i<group_num; i++)
	{
		FlKeyGroup* key_group = m_interpolator.GetKeyGroup(i);
		if (key_group)
		{
			str = wxString::Format("/interpolator/%d", i);
			fconfig.SetPath(str);
			fconfig.Write("id", key_group->id);
			fconfig.Write("t", key_group->t);
			fconfig.Write("type", key_group->type);
			str = key_group->desc;
			fconfig.Write("desc", str);
			int key_num = (int)key_group->keys.size();
			str = wxString::Format("/interpolator/%d/keys", i);
			fconfig.SetPath(str);
			fconfig.Write("num", key_num);
			for (j=0; j<key_num; j++)
			{
				FlKey* key = key_group->keys[j];
				if (key)
				{
					str = wxString::Format("/interpolator/%d/keys/%d", i, j);
					fconfig.SetPath(str);
					int key_type = key->GetType();
					fconfig.Write("type", key_type);
					KeyCode code = key->GetKeyCode();
					fconfig.Write("l0", code.l0);
					str = code.l0_name;
					fconfig.Write("l0_name", str);
					fconfig.Write("l1", code.l1);
					str = code.l1_name;
					fconfig.Write("l1_name", str);
					fconfig.Write("l2", code.l2);
					str = code.l2_name;
					fconfig.Write("l2_name", str);
					switch (key_type)
					{
					case FLKEY_TYPE_DOUBLE:
						{
							double dval = ((FlKeyDouble*)key)->GetValue();
							fconfig.Write("val", dval);
						}
						break;
					case FLKEY_TYPE_QUATER:
						{
							Quaternion qval = ((FlKeyQuaternion*)key)->GetValue();
							str = wxString::Format("%lf %lf %lf %lf",
								qval.x, qval.y, qval.z, qval.w);
							fconfig.Write("val", str);
						}
						break;
					case FLKEY_TYPE_BOOLEAN:
						{
							bool bval = ((FlKeyBoolean*)key)->GetValue();
							fconfig.Write("val", bval);
						}
						break;
					}
				}
			}
		}
	}

	wxFileOutputStream os(filename);
	fconfig.Save(os);
	
	delete prg_diag;
}

VolumeData* VRenderFrame::OpenVolumeFromProject(wxString name, wxFileConfig &fconfig)
{
	int iVal;
	VolumeData *vd = NULL;
	
	wxString tmp_path = fconfig.GetPath();
	fconfig.SetPath("/");

	wxString ver_major, ver_minor;
	long l_major, l_minor;
	l_major = 1;
	if (fconfig.Read("ver_major", &ver_major) &&
		fconfig.Read("ver_minor", &ver_minor))
	{
		ver_major.ToLong(&l_major);
		ver_minor.ToLong(&l_minor);
	}

	if (fconfig.Exists("/data/volume"))
	{
		fconfig.SetPath("/data/volume");
		int num = fconfig.Read("num", 0l);
		for (int i=0; i<num; i++)
		{
			wxString str;
			str = wxString::Format("/data/volume/%d", i);
			if (fconfig.Exists(str))
			{
				int loaded_num = 0;
				fconfig.SetPath(str);
				if (!fconfig.Read("name", &str))
					continue;
				if (str != name)
					continue;
				
				bool compression = false;
				fconfig.Read("compression", &compression);
				m_data_mgr.SetCompression(compression);
				bool skip_brick = false;
				fconfig.Read("skip_brick", &skip_brick);
				m_data_mgr.SetSkipBrick(skip_brick);
				//path
				if (fconfig.Read("path", &str))
				{
					int cur_chan = 0;
					if (!fconfig.Read("cur_chan", &cur_chan))
						if (fconfig.Read("tiff_chan", &cur_chan))
							cur_chan--;
					int cur_time = 0;
					fconfig.Read("cur_time", &cur_time);
					bool slice_seq = 0;
					fconfig.Read("slice_seq", &slice_seq);
					m_data_mgr.SetSliceSequence(slice_seq);
					wxString time_id;
					fconfig.Read("time_id", &time_id);
					m_data_mgr.SetTimeId(time_id);
					wxString suffix = str.Mid(str.Find('.', true)).MakeLower();
					if (suffix == ".nrrd")
						loaded_num = m_data_mgr.LoadVolumeData(str, LOAD_TYPE_NRRD, cur_chan, cur_time);
					else if (suffix == ".tif"||suffix == ".tiff")
						loaded_num = m_data_mgr.LoadVolumeData(str, LOAD_TYPE_TIFF, cur_chan, cur_time);
					else if (suffix == ".oib")
						loaded_num = m_data_mgr.LoadVolumeData(str, LOAD_TYPE_OIB, cur_chan, cur_time);
					else if (suffix == ".oif")
						loaded_num = m_data_mgr.LoadVolumeData(str, LOAD_TYPE_OIF, cur_chan, cur_time);
					else if (suffix == ".lsm")
						loaded_num = m_data_mgr.LoadVolumeData(str, LOAD_TYPE_LSM, cur_chan, cur_time);
					else if (suffix == ".xml")
						loaded_num = m_data_mgr.LoadVolumeData(str, LOAD_TYPE_PVXML, cur_chan, cur_time);
					else if (suffix == ".vvd")
						loaded_num = m_data_mgr.LoadVolumeData(str, LOAD_TYPE_BRKXML, cur_chan, cur_time);
				}
				if (loaded_num)
					vd = m_data_mgr.GetLastVolumeData();
				if (vd)
				{
					if (fconfig.Read("name", &str))
						vd->SetName(str);//setname
					//volume properties
					if (fconfig.Exists("properties"))
					{
						fconfig.SetPath("properties");
						bool disp;
						if (fconfig.Read("display", &disp))
							vd->SetDisp(disp);

						//old colormap
						if (fconfig.Read("widget", &str))
						{
							int type;
							float left_x, left_y, width, height, offset1, offset2, gamma;
							wchar_t token[256];
							token[255] = '\0';
							const wchar_t* sstr = str.wc_str();
							std::wstringstream ss(sstr);
							ss.read(token,255);
							wchar_t c = 'x';
							while(!isspace(c)) ss.read(&c,1);
							ss >> type >> left_x >> left_y >> width >>
								height >> offset1 >> offset2 >> gamma;
							vd->Set3DGamma(gamma);
							vd->SetBoundary(left_y);
							vd->SetOffset(offset1);
							vd->SetLeftThresh(left_x);
							vd->SetRightThresh(left_x+width);
							if (fconfig.Read("widgetcolor", &str))
							{
								float red, green, blue;
								if (SSCANF(str.c_str(), "%f%f%f", &red, &green, &blue)){
									FLIVR::Color col(red,green,blue);
									vd->SetColor(col);
								}
							}
							double alpha;
							if (fconfig.Read("widgetalpha", &alpha))
								vd->SetAlpha(alpha);
						}

						//transfer function
						double dval;
						bool bval;
						if (fconfig.Read("3dgamma", &dval))
							vd->Set3DGamma(dval);
						if (fconfig.Read("boundary", &dval))
							vd->SetBoundary(dval);
						if (fconfig.Read("contrast", &dval))
							vd->SetOffset(dval);
						if (fconfig.Read("left_thresh", &dval))
							vd->SetLeftThresh(dval);
						if (fconfig.Read("right_thresh", &dval))
							vd->SetRightThresh(dval);
						if (fconfig.Read("color", &str))
						{
							float red, green, blue;
							if (SSCANF(str.c_str(), "%f%f%f", &red, &green, &blue)){
								FLIVR::Color col(red,green,blue);
								vd->SetColor(col);
							}
						}
						if (fconfig.Read("hsv", &str))
						{
							float hue, sat, val;
							if (SSCANF(str.c_str(), "%f%f%f", &hue, &sat, &val))
								vd->SetHSV(hue, sat, val);
						}
						if (fconfig.Read("mask_color", &str))
						{
							float red, green, blue;
							if (SSCANF(str.c_str(), "%f%f%f", &red, &green, &blue)){
								FLIVR::Color col(red,green,blue);
								if (fconfig.Read("mask_color_set", &bval))
									vd->SetMaskColor(col, bval);
								else
									vd->SetMaskColor(col);
							}
						}
						if (fconfig.Read("enable_alpha", &bval))
							vd->SetEnableAlpha(bval);
						if (fconfig.Read("alpha", &dval))
							vd->SetAlpha(dval);

						//shading
						double amb, diff, spec, shine;
						if (fconfig.Read("ambient", &amb)&&
							fconfig.Read("diffuse", &diff)&&
							fconfig.Read("specular", &spec)&&
							fconfig.Read("shininess", &shine))
							vd->SetMaterial(amb, diff, spec, shine);
						bool shading;
						if (fconfig.Read("shading", &shading))
							vd->SetShading(shading);
						double srate;
						if (fconfig.Read("samplerate", &srate))
						{
							if (l_major<2)
								vd->SetSampleRate(srate/5.0);
							else
								vd->SetSampleRate(srate);
						}

						//spacings and scales
						if (!vd->isBrxml())
						{
							if (fconfig.Read("res", &str))
							{
								double resx, resy, resz;
								if (SSCANF(str.c_str(), "%lf%lf%lf", &resx, &resy, &resz))
									vd->SetSpacings(resx, resy, resz);
							}
						}
						else
						{
							if (fconfig.Read("b_res", &str))
							{
								double b_resx, b_resy, b_resz;
								if (SSCANF(str.c_str(), "%lf%lf%lf", &b_resx, &b_resy, &b_resz))
									vd->SetBaseSpacings(b_resx, b_resy, b_resz);
							}
							if (fconfig.Read("s_res", &str))
							{
								double s_resx, s_resy, s_resz;
								if (SSCANF(str.c_str(), "%lf%lf%lf", &s_resx, &s_resy, &s_resz))
									vd->SetSpacingScales(s_resx, s_resy, s_resz);
							}
						}
						if (fconfig.Read("scl", &str))
						{
							double sclx, scly, sclz;
							if (SSCANF(str.c_str(), "%lf%lf%lf", &sclx, &scly, &sclz))
								vd->SetScalings(sclx, scly, sclz);
						}

						vector<Plane*> *planes = 0;
						if (vd->GetVR())
							planes = vd->GetVR()->get_planes();
						int iresx, iresy, iresz;
						vd->GetResolution(iresx, iresy, iresz);
						if (planes && planes->size()==6)
						{
							double val;
							wxString splane;

							//x1
							if (fconfig.Read("x1_vali", &val))
								(*planes)[0]->ChangePlane(Point(abs(val/iresx), 0.0, 0.0),
								Vector(1.0, 0.0, 0.0));
							else if (fconfig.Read("x1_val", &val))
								(*planes)[0]->ChangePlane(Point(abs(val), 0.0, 0.0),
								Vector(1.0, 0.0, 0.0));

							//x2
							if (fconfig.Read("x2_vali", &val))
								(*planes)[1]->ChangePlane(Point(abs(val/iresx), 0.0, 0.0),
								Vector(-1.0, 0.0, 0.0));
							else if (fconfig.Read("x2_val", &val))
								(*planes)[1]->ChangePlane(Point(abs(val), 0.0, 0.0),
								Vector(-1.0, 0.0, 0.0));

							//y1
							if (fconfig.Read("y1_vali", &val))
								(*planes)[2]->ChangePlane(Point(0.0, abs(val/iresy), 0.0),
								Vector(0.0, 1.0, 0.0));
							else if (fconfig.Read("y1_val", &val))
								(*planes)[2]->ChangePlane(Point(0.0, abs(val), 0.0),
								Vector(0.0, 1.0, 0.0));

							//y2
							if (fconfig.Read("y2_vali", &val))
								(*planes)[3]->ChangePlane(Point(0.0, abs(val/iresy), 0.0),
								Vector(0.0, -1.0, 0.0));
							else if (fconfig.Read("y2_val", &val))
								(*planes)[3]->ChangePlane(Point(0.0, abs(val), 0.0),
								Vector(0.0, -1.0, 0.0));

							//z1
							if (fconfig.Read("z1_vali", &val))
								(*planes)[4]->ChangePlane(Point(0.0, 0.0, abs(val/iresz)),
								Vector(0.0, 0.0, 1.0));
							else if (fconfig.Read("z1_val", &val))
								(*planes)[4]->ChangePlane(Point(0.0, 0.0, abs(val)),
								Vector(0.0, 0.0, 1.0));

							//z2
							if (fconfig.Read("z2_vali", &val))
								(*planes)[5]->ChangePlane(Point(0.0, 0.0, abs(val/iresz)),
								Vector(0.0, 0.0, -1.0));
							else if (fconfig.Read("z2_val", &val))
								(*planes)[5]->ChangePlane(Point(0.0, 0.0, abs(val)),
								Vector(0.0, 0.0, -1.0));
						}

						//2d adjustment settings
						if (fconfig.Read("gamma", &str))
						{
							float r, g, b;
							if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
								FLIVR::Color col(r,g,b);
								vd->SetGamma(col);
							}
						}
						if (fconfig.Read("brightness", &str))
						{
							float r, g, b;
							if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
								FLIVR::Color col(r,g,b);
								vd->SetBrightness(col);
							}
						}
						if (fconfig.Read("hdr", &str))
						{
							float r, g, b;
							if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
								FLIVR::Color col(r,g,b);
								vd->SetHdr(col);
							}
						}
						bool bVal;
						if (fconfig.Read("sync_r", &bVal))
							vd->SetSyncR(bVal);
						if (fconfig.Read("sync_g", &bVal))
							vd->SetSyncG(bVal);
						if (fconfig.Read("sync_b", &bVal))
							vd->SetSyncB(bVal);

						//colormap settings
						if (fconfig.Read("colormap_mode", &iVal))
							vd->SetColormapMode(iVal);
						if (fconfig.Read("colormap", &iVal))
							vd->SetColormap(iVal);
						double low, high;
						if (fconfig.Read("colormap_lo_value", &low) &&
							fconfig.Read("colormap_hi_value", &high))
						{
							vd->SetColormapValues(low, high);
						}
						if (fconfig.Read("id_color_disp_mode", &iVal))
							vd->SetIDColDispMode(iVal);

						//inversion
						if (fconfig.Read("inv", &bVal))
							vd->SetInvert(bVal);
						//mip enable
						if (fconfig.Read("mode", &iVal))
							vd->SetMode(iVal);
						//noise reduction
						if (fconfig.Read("noise_red", &bVal))
							vd->SetNR(bVal);
						//depth override
						if (fconfig.Read("depth_ovrd", &iVal))
							vd->SetBlendMode(iVal);

						//shadow
						if (fconfig.Read("shadow", &bVal))
							vd->SetShadow(bVal);
						//shaodw intensity
						if (fconfig.Read("shadow_darkness", &dval))
							vd->SetShadowParams(dval);

						//legend
						if (fconfig.Read("legend", &bVal))
							vd->SetLegend(bVal);

						//roi
						if (fconfig.Read("roi_tree", &str))
							vd->ImportROITree(str.ToStdWstring());
						if (fconfig.Read("selected_rois", &str))
							vd->ImportSelIDs(str.ToStdString());
						if (fconfig.Read("roi_disp_mode", &iVal))
							vd->SetIDColDispMode(iVal);

						//mask
						if (fconfig.Read("mask", &str))
						{
							MSKReader msk_reader;
							wstring maskname = str.ToStdWstring();
							msk_reader.SetFile(maskname);
							BaseReader *br = &msk_reader;
							Nrrd* mask = br->Convert(true);
							if (mask)
								vd->LoadMask(mask);
						}
					}
				}
			}
		}
	}
	fconfig.SetPath(tmp_path);
	return vd;
}

MeshData* VRenderFrame::OpenMeshFromProject(wxString name, wxFileConfig &fconfig)
{
	MeshData *md = NULL;

	wxString tmp_path = fconfig.GetPath();
	fconfig.SetPath("/");

	if (fconfig.Exists("/data/mesh"))
	{
		fconfig.SetPath("/data/mesh");
		int num = fconfig.Read("num", 0l);
		for (int i=0; i<num; i++)
		{
			wxString str;
			str = wxString::Format("/data/mesh/%d", i);
			if (fconfig.Exists(str))
			{
				fconfig.SetPath(str);
				if (!fconfig.Read("name", &str))
					continue;
				if (str != name)
					continue;

				if (fconfig.Read("path", &str))
				{
					m_data_mgr.LoadMeshData(str);
				}
				MeshData* md = m_data_mgr.GetLastMeshData();
				if (md)
				{
					if (fconfig.Read("name", &str))
						md->SetName(str);//setname
					//mesh properties
					if (fconfig.Exists("properties"))
					{
						fconfig.SetPath("properties");
						bool disp;
						if (fconfig.Read("display", &disp))
							md->SetDisp(disp);
						//lighting
						bool lighting;
						if (fconfig.Read("lighting", &lighting))
							md->SetLighting(lighting);
						double shine, alpha;
						float r=0.0f, g=0.0f, b=0.0f;
						if (fconfig.Read("ambient", &str))
							SSCANF(str.c_str(), "%f%f%f", &r, &g, &b);
						Color amb(r, g, b);
						if (fconfig.Read("diffuse", &str))
							SSCANF(str.c_str(), "%f%f%f", &r, &g, &b);
						Color diff(r, g, b);
						if (fconfig.Read("specular", &str))
							SSCANF(str.c_str(), "%f%f%f", &r, &g, &b);
						Color spec(r, g, b);
						fconfig.Read("shininess", &shine, 30.0);
						fconfig.Read("alpha", &alpha, 0.5);
						md->SetMaterial(amb, diff, spec, shine, alpha);
						//2d adjusment settings
						if (fconfig.Read("gamma", &str))
						{
							float r, g, b;
							if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
								FLIVR::Color col(r,g,b);
								md->SetGamma(col);
							}
						}
						if (fconfig.Read("brightness", &str))
						{
							float r, g, b;
							if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
								FLIVR::Color col(r,g,b);
								md->SetBrightness(col);
							}
						}
						if (fconfig.Read("hdr", &str))
						{
							float r, g, b;
							if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
								FLIVR::Color col(r,g,b);
								md->SetHdr(col);
							}
						}
						bool bVal;
						if (fconfig.Read("sync_r", &bVal))
							md->SetSyncG(bVal);
						if (fconfig.Read("sync_g", &bVal))
							md->SetSyncG(bVal);
						if (fconfig.Read("sync_b", &bVal))
							md->SetSyncB(bVal);
						//shadow
						if (fconfig.Read("shadow", &bVal))
							md->SetShadow(bVal);
						double darkness;
						if (fconfig.Read("shadow_darkness", &darkness))
							md->SetShadowParams(darkness);

						//mesh transform
						if (fconfig.Exists("../transform"))
						{
							fconfig.SetPath("../transform");
							float x, y, z;
							if (fconfig.Read("translation", &str))
							{
								if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
									md->SetTranslation(x, y, z);
							}
							if (fconfig.Read("rotation", &str))
							{
								if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
									md->SetRotation(x, y, z);
							}
							if (fconfig.Read("scaling", &str))
							{
								if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
									md->SetScaling(x, y, z);
							}
						}

					}
				}
			}
		}
	}
	fconfig.SetPath(tmp_path);
	return md;
}

void VRenderFrame::OpenProject(wxString& filename)
{
	m_data_mgr.SetProjectPath(filename);
    
    SetEvtHandlerEnabled(false);
    //Freeze();

	int iVal;
	int i, j, k;
	//clear
	m_data_mgr.ClearAll();
	DataGroup::ResetID();
	MeshGroup::ResetID();
	m_adjust_view->SetVolumeData(0);
	m_adjust_view->SetGroup(0);
	m_adjust_view->SetGroupLink(0);
	m_vrv_list[0]->Clear();
	for (i=m_vrv_list.size()-1; i>0; i--)
		DeleteVRenderView(i);
	
	wxFileInputStream is(filename);
	if (!is.IsOk())
		return;
	wxFileConfig fconfig(is);
	wxString ver_major, ver_minor;
	long l_major, l_minor;
	l_major = 1;
	if (fconfig.Read("ver_major", &ver_major) &&
		fconfig.Read("ver_minor", &ver_minor))
	{
		ver_major.ToLong(&l_major);
		ver_minor.ToLong(&l_minor);

		if (l_major>VERSION_MAJOR)
			::wxMessageBox("The project file is saved by a newer version of FluoRender.\n" \
			"Please check update and download the new version.");
		else if (l_minor>VERSION_MINOR)
			::wxMessageBox("The project file is saved by a newer version of FluoRender.\n" \
			"Please check update and download the new version.");
	}

	int ticks = 0;
	int tick_cnt = 1;
	fconfig.Read("ticks", &ticks);
	wxProgressDialog *prg_diag = 0;
	prg_diag = new wxProgressDialog(
		"FluoRender: Loading project...",
		"Reading project file. Please wait.",
		100, 0, wxPD_SMOOTH|wxPD_ELAPSED_TIME|wxPD_AUTO_HIDE);

	//read streaming mode
	if (fconfig.Exists("/memory settings"))
	{
		fconfig.SetPath("/memory settings");
		bool mem_swap = false;
		fconfig.Read("mem swap", &mem_swap);
		double graphics_mem = 1000.0;
		fconfig.Read("graphics mem", &graphics_mem);
		double large_data_size = 1000.0;
		fconfig.Read("large data size", &large_data_size);
		int force_brick_size = 128;
		fconfig.Read("force brick size", &force_brick_size);
		int up_time = 100;
		fconfig.Read("up time", &up_time);
		int update_order = 0;
		fconfig.Read("update order", &update_order);

		m_setting_dlg->SetMemSwap(mem_swap);
		m_setting_dlg->SetGraphicsMem(graphics_mem);
		m_setting_dlg->SetLargeDataSize(large_data_size);
		m_setting_dlg->SetForceBrickSize(force_brick_size);
		m_setting_dlg->SetResponseTime(up_time);
		m_setting_dlg->SetUpdateOrder(update_order);
		m_setting_dlg->UpdateUI();

		SetTextureRendererSettings();
	}

	//annotations
	if (fconfig.Exists("/data/annotations"))
	{
		fconfig.SetPath("/data/annotations");
		int num = fconfig.Read("num", 0l);
		for (i=0; i<num; i++)
		{
			wxString str;
			str = wxString::Format("/data/annotations/%d", i);
			if (fconfig.Exists(str))
			{
				fconfig.SetPath(str);
				if (fconfig.Read("path", &str))
				{
					m_data_mgr.LoadAnnotations(str);
				}
			}
		}
	}

	bool bVal;
	//views
	if (fconfig.Exists("/views"))
	{
		fconfig.SetPath("/views");
		int num = fconfig.Read("num", 0l);

		//m_vrv_list[0]->Clear();
		//for (i=m_vrv_list.size()-1; i>0; i--)
		//	DeleteVRenderView(i);
		//VRenderView::ResetID();
		DataGroup::ResetID();
		MeshGroup::ResetID();

		for (i=0; i<num; i++)
		{
			if (i>0)
				CreateView();
			VRenderView* vrv = GetLastView();
			if (!vrv)
				continue;

			vrv->Clear();

			if (i==0 && m_setting_dlg && m_setting_dlg->GetTestMode(1))
				vrv->m_glview->m_test_speed = true;

			if (ticks && prg_diag)
				prg_diag->Update(90*tick_cnt/ticks,
				"Reading project file. Please wait.");

			wxString str;
			//old
			//volumes
			str = wxString::Format("/views/%d/volumes", i);
			if (fconfig.Exists(str))
			{
				fconfig.SetPath(str);
				int num = fconfig.Read("num", 0l);
				for (j=0; j<num; j++)
				{
					if (fconfig.Read(wxString::Format("name%d", j), &str))
					{
						
						VolumeData* vd = OpenVolumeFromProject(str, fconfig);
						if (vd)
							vrv->AddVolumeData(vd);
						tick_cnt++;
					}
				}
				vrv->SetVolPopDirty();
			}
			//meshes
			str = wxString::Format("/views/%d/meshes", i);
			if (fconfig.Exists(str))
			{
				fconfig.SetPath(str);
				int num = fconfig.Read("num", 0l);
				for (j=0; j<num; j++)
				{
					if (fconfig.Read(wxString::Format("name%d", j), &str))
					{
						MeshData* md = OpenMeshFromProject(str, fconfig);
						if (md)
							vrv->AddMeshData(md);
						tick_cnt++;
					}
				}
			}

			//new
			str = wxString::Format("/views/%d/layers", i);
			if (fconfig.Exists(str))
			{
				fconfig.SetPath(str);

				//view layers
				int layer_num = fconfig.Read("num", 0l);
				for (j=0; j<layer_num; j++)
				{
					if (fconfig.Exists(wxString::Format("/views/%d/layers/%d", i, j)))
					{
						fconfig.SetPath(wxString::Format("/views/%d/layers/%d", i, j));
						int type;
						if (fconfig.Read("type", &type))
						{
							switch (type)
							{
							case 2://volume data
								{
									if (fconfig.Read("name", &str))
									{
										VolumeData* vd = OpenVolumeFromProject(str, fconfig);
										if (vd)
											vrv->AddVolumeData(vd);
										tick_cnt++;
									}
								}
								break;
							case 3://mesh data
								{
									if (fconfig.Read("name", &str))
									{
										MeshData* md = OpenMeshFromProject(str, fconfig);
										if (md)
											vrv->AddMeshData(md);
										tick_cnt++;
									}
								}
								break;
							case 4://annotations
								{
									if (fconfig.Read("name", &str))
									{
										Annotations* ann = m_data_mgr.GetAnnotations(str);
										if (ann)
											vrv->AddAnnotations(ann);
									}
								}
								break;
							case 5://group
								{
									if (fconfig.Read("name", &str))
									{
										int id;
										if (fconfig.Read("id", &id))
											DataGroup::SetID(id-1);
										str = vrv->AddGroup(str);
										DataGroup* group = vrv->GetGroup(str);
										if (group)
										{
											//display
											if (fconfig.Read("display", &bVal))
											{
												group->SetDisp(bVal);
											}
											//2d adjustment
											if (fconfig.Read("gamma", &str))
											{
												float r, g, b;
												if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
													FLIVR::Color col(r,g,b);
													group->SetGamma(col);
												}
											}
											if (fconfig.Read("brightness", &str))
											{
												float r, g, b;
												if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
													FLIVR::Color col(r,g,b);
													group->SetBrightness(col);
												}
											}
											if (fconfig.Read("hdr", &str))
											{
												float r, g, b;
												if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
													FLIVR::Color col(r,g,b);
													group->SetHdr(col);
												}
											}
											if (fconfig.Read("sync_r", &bVal))
												group->SetSyncR(bVal);
											if (fconfig.Read("sync_g", &bVal))
												group->SetSyncG(bVal);
											if (fconfig.Read("sync_b", &bVal))
												group->SetSyncB(bVal);
											//sync volume properties
											if (fconfig.Read("sync_vp", &bVal))
												group->SetVolumeSyncProp(bVal);
											//sync volume properties
											if (fconfig.Read("sync_vspc", &bVal))
												group->SetVolumeSyncSpc(bVal);
											//volumes
											if (fconfig.Exists(wxString::Format("/views/%d/layers/%d/volumes", i, j)))
											{
												fconfig.SetPath(wxString::Format("/views/%d/layers/%d/volumes", i, j));
												int vol_num = fconfig.Read("num", 0l);
												for (k=0; k<vol_num; k++)
												{
													if (fconfig.Read(wxString::Format("vol_%d", k), &str))
													{
														VolumeData* vd = OpenVolumeFromProject(str, fconfig);
														if (vd)
															group->InsertVolumeData(k-1, vd);
														tick_cnt++;
													}
												}
											}
										}
										vrv->SetVolPopDirty();
									}
								}
								break;
							case 6://mesh group
								{
									if (fconfig.Read("name", &str))
									{
										int id;
										if (fconfig.Read("id", &id))
											MeshGroup::SetID(id);
										str = vrv->AddMGroup(str);
										MeshGroup* group = vrv->GetMGroup(str);
										if (group)
										{
											//display
											if (fconfig.Read("display", &bVal))
												group->SetDisp(bVal);
											//sync mesh properties
											if (fconfig.Read("sync_mp", &bVal))
												group->SetMeshSyncProp(bVal);
											//meshes
											if (fconfig.Exists(wxString::Format("/views/%d/layers/%d/meshes", i, j)))
											{
												fconfig.SetPath(wxString::Format("/views/%d/layers/%d/meshes", i, j));
												int mesh_num = fconfig.Read("num", 0l);
												for (k=0; k<mesh_num; k++)
												{
													if (fconfig.Read(wxString::Format("mesh_%d", k), &str))
													{
														MeshData* md = OpenMeshFromProject(str, fconfig);
														if (md)
															group->InsertMeshData(k-1, md);
														tick_cnt++;
													}
												}
											}
										}
										vrv->SetMeshPopDirty();
									}
								}
								break;
							}
						}
					}
				}
			}

			//properties
			if (fconfig.Exists(wxString::Format("/views/%d/properties", i)))
			{
				float x, y, z;
				fconfig.SetPath(wxString::Format("/views/%d/properties", i));
				bool draw;
				if (fconfig.Read("drawall", &draw))
					vrv->SetDraw(draw);
				//properties
				bool persp;
				if (fconfig.Read("persp", &persp))
					vrv->SetPersp(persp);
				else
					vrv->SetPersp(true);
				bool free;
				if (fconfig.Read("free", &free))
					vrv->SetFree(free);
				else
					vrv->SetFree(false);
				double aov;
				if (fconfig.Read("aov", &aov))
					vrv->SetAov(aov);
				double dpi;
				if (fconfig.Read("min_dpi", &dpi))
					vrv->SetMinPPI(dpi);
				int res_mode;
				if (fconfig.Read("res_mode", &res_mode))
					vrv->SetResMode(res_mode);
				double nearclip;
				if (fconfig.Read("nearclip", &nearclip))
					vrv->SetNearClip(nearclip);
				double farclip;
				if (fconfig.Read("farclip", &farclip))
					vrv->SetFarClip(farclip);
				if (fconfig.Read("backgroundcolor", &str))
				{
					float r, g, b;
					if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
						FLIVR::Color col(r,g,b);
						vrv->SetBackgroundColor(col);
					}
				}
				int volmethod;
				if (fconfig.Read("volmethod", &volmethod))
					vrv->SetVolMethod(volmethod);
				int peellayers;
				if (fconfig.Read("peellayers", &peellayers))
					vrv->SetPeelingLayers(peellayers);
				bool fog;
				if (fconfig.Read("fog", &fog))
					vrv->SetFog(fog);
				double fogintensity;
				if (fconfig.Read("fogintensity", &fogintensity))
					vrv->m_depth_atten_factor_text->SetValue(wxString::Format("%.2f",fogintensity));
				if (fconfig.Read("draw_camctr", &bVal))
				{
					vrv->m_glview->m_draw_camctr = bVal;
					vrv->m_cam_ctr_chk->SetValue(bVal);
				}
				if (fconfig.Read("draw_fps", &bVal))
				{
					vrv->m_glview->m_draw_info = bVal;
					vrv->m_fps_chk->SetValue(bVal);
				}
				if (fconfig.Read("draw_legend", &bVal))
				{
					vrv->m_glview->m_draw_legend = bVal;
					vrv->m_legend_chk->SetValue(bVal);
				}

				//camera
				if (fconfig.Read("translation", &str))
				{
					if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
						vrv->SetTranslations(x, y, z);
				}
				if (fconfig.Read("rotation", &str))
				{
					if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
						vrv->SetRotations(x, y, z);
				}
				if (fconfig.Read("center", &str))
				{
					if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
						vrv->SetCenters(x, y, z);
				}
				double dist;
				if (fconfig.Read("centereyedist", &dist))
					vrv->SetCenterEyeDist(dist);
				double radius = 5.0;
				if (fconfig.Read("radius", &radius))
					vrv->SetRadius(radius);
				double initdist;
				if (fconfig.Read("initdist", &initdist))
					vrv->m_glview->SetInitDist(initdist);
				else
					vrv->m_glview->SetInitDist(radius/tan(d2r(vrv->GetAov()/2.0)));
				double scale;
				if (fconfig.Read("scale", &scale))
					vrv->SetScaleFactor(scale);
				else
					vrv->SetScaleFactor(radius/tan(d2r(vrv->GetAov()/2.0))/dist);
				//object
				if (fconfig.Read("obj_center", &str))
				{
					if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
						vrv->SetObjCenters(x, y, z);
				}
				if (fconfig.Read("obj_trans", &str))
				{
					if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
						vrv->SetObjTrans(x, y, z);
				}
				if (fconfig.Read("obj_rot", &str))
				{
					if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z))
						vrv->SetObjRot(x, y, z);
				}
				//scale bar
				bool disp;
				if (fconfig.Read("disp_scale_bar", &disp))
					vrv->m_glview->m_disp_scale_bar = disp;
				if (fconfig.Read("disp_scale_bar_text", &disp))
					vrv->m_glview->m_disp_scale_bar_text = disp;
				double length;
				if (fconfig.Read("sb_length", &length))
					vrv->m_glview->m_sb_length = length;
				if (fconfig.Read("sb_text", &str))
					vrv->m_glview->m_sb_text = str;
				if (fconfig.Read("sb_num", &str))
					vrv->m_glview->m_sb_num = str;
				int unit;
				if (fconfig.Read("sb_unit", &unit))
					vrv->m_glview->m_sb_unit = unit;

				//2d sdjustment settings
				if (fconfig.Read("gamma", &str))
				{
					float r, g, b;
					if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
						FLIVR::Color col(r,g,b);
						vrv->m_glview->SetGamma(col);
					}
				}
				if (fconfig.Read("brightness", &str))
				{
					float r, g, b;
					if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
						FLIVR::Color col(r,g,b);
						vrv->m_glview->SetBrightness(col);
					}
				}
				if (fconfig.Read("hdr", &str))
				{
					float r, g, b;
					if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b)){
						FLIVR::Color col(r,g,b);
						vrv->m_glview->SetHdr(col);
					}
				}
				if (fconfig.Read("sync_r", &bVal))
					vrv->m_glview->SetSyncR(bVal);
				if (fconfig.Read("sync_g", &bVal))
					vrv->m_glview->SetSyncG(bVal);
				if (fconfig.Read("sync_b", &bVal))
					vrv->m_glview->SetSyncB(bVal);

				//clipping plane rotations
				int clip_mode;
				if (fconfig.Read("clip_mode", &clip_mode))
					vrv->SetClipMode(clip_mode);
				double rotx_cl, roty_cl, rotz_cl;
				if (fconfig.Read("rotx_cl", &rotx_cl) &&
					fconfig.Read("roty_cl", &roty_cl) &&
					fconfig.Read("rotz_cl", &rotz_cl))
				{
					vrv->SetClippingPlaneRotations(rotx_cl, roty_cl, rotz_cl);
					m_clip_view->SetClippingPlaneRotations(rotx_cl, roty_cl, rotz_cl);
				}

				//painting parameters
				double dVal;
				if (fconfig.Read("brush_use_pres", &bVal))
					vrv->SetBrushUsePres(bVal);
				double size1, size2;
				if (fconfig.Read("brush_size_1", &size1) &&
					fconfig.Read("brush_size_2", &size2))
					vrv->SetBrushSize(size1, size2);
				if (fconfig.Read("brush_spacing", &dVal))
					vrv->SetBrushSpacing(dVal);
				if (fconfig.Read("brush_iteration", &dVal))
					vrv->SetBrushIteration(dVal);
				if (fconfig.Read("brush_translate", &dVal))
					vrv->SetBrushSclTranslate(dVal);
				if (fconfig.Read("w2d", &dVal))
					vrv->SetW2d(dVal);

				//rulers
				if (vrv->GetRulerList())
				{
					vrv->GetRulerList()->clear();
					if (fconfig.Exists(wxString::Format("/views/%d/rulers", i)))
					{
						fconfig.SetPath(wxString::Format("/views/%d/rulers", i));
						int rnum = fconfig.Read("num", 0l);
						for (int ri=0; ri<rnum; ++ri)
						{
							if (fconfig.Exists(wxString::Format("/views/%d/rulers/%d", i, ri)))
							{
								fconfig.SetPath(wxString::Format("/views/%d/rulers/%d", i, ri));
								Ruler* ruler = new Ruler();
								if (fconfig.Read("name", &str))
									ruler->SetName(str);
								if (fconfig.Read("name_disp", &str))
									ruler->SetNameDisp(str);
								if (fconfig.Read("description", &str))
									ruler->SetDesc(str);
								if (fconfig.Read("type", &iVal))
									ruler->SetRulerType(iVal);
								if (fconfig.Read("display", &bVal))
									ruler->SetDisp(bVal);
								if (fconfig.Read("transient", &bVal))
									ruler->SetTimeDep(bVal);
								if (fconfig.Read("time", &iVal))
									ruler->SetTime(iVal);
								if (fconfig.Read("info_names", &str))
									ruler->SetInfoNames(str);
								if (fconfig.Read("info_values", &str))
									ruler->SetInfoValues(str);
								if (fconfig.Read("use_color", &bVal))
								{
									if (bVal)
									{
										if (fconfig.Read("color", &str))
										{
											float r, g, b;
											if (SSCANF(str.c_str(), "%f%f%f", &r, &g, &b))
											{
												FLIVR::Color col(r,g,b);
												ruler->SetColor(col);
											}
										}
									}
								}
								int pnum = fconfig.Read("num", 0l);
								for (int rpi=0; rpi<pnum; ++rpi)
								{
									if (fconfig.Exists(wxString::Format("/views/%d/rulers/%d/points/%d", i, ri, rpi)))
									{
										fconfig.SetPath(wxString::Format("/views/%d/rulers/%d/points/%d", i, ri, rpi));
										if (fconfig.Read("point", &str))
										{
											float x, y, z;
											if (SSCANF(str.c_str(), "%f%f%f", &x, &y, &z)){
												Point point(x,y,z);
												ruler->AddPoint(point);
											}
										}
									}
								}
								vrv->GetRulerList()->push_back(ruler);
							}
						}
					}
				}
			}
		}
	}

	//current selected volume
	if (fconfig.Exists("/prop_panel"))
	{
		fconfig.SetPath("/prop_panel");
		int cur_sel_type, cur_sel_vol, cur_sel_mesh;
		if (fconfig.Read("cur_sel_type", &cur_sel_type) &&
			fconfig.Read("cur_sel_vol", &cur_sel_vol) &&
			fconfig.Read("cur_sel_mesh", &cur_sel_mesh))
		{
			m_cur_sel_type = cur_sel_type;
			m_cur_sel_vol = cur_sel_vol;
			m_cur_sel_mesh = cur_sel_mesh;
			switch (m_cur_sel_type)
			{
			case 2:  //volume
				OnSelection(2, 0, 0, m_data_mgr.GetVolumeData(cur_sel_vol));
				break;
			case 3:  //mesh
				OnSelection(3, 0, 0, 0, m_data_mgr.GetMeshData(cur_sel_mesh));
				break;
			}
		}
		else if (fconfig.Read("cur_sel_vol", &cur_sel_vol))
		{
			m_cur_sel_vol = cur_sel_vol;
			if (m_cur_sel_vol != -1)
			{
				VolumeData* vd = m_data_mgr.GetVolumeData(m_cur_sel_vol);
				OnSelection(2, 0, 0, vd);
			}
		}
		bool link;
		int mode;
		if (fconfig.Read("chann_link", &link))
			m_clip_view->SetChannLink(link);
		if (fconfig.Read("plane_mode", &mode))
			m_clip_view->SetPlaneMode(mode);
		if (fconfig.Read("x_link", &link))
			m_clip_view->SetXLink(link);
		if (fconfig.Read("y_link", &link))
			m_clip_view->SetYLink(link);
		if (fconfig.Read("z_link", &link))
			m_clip_view->SetZLink(link);
	}

	//movie panel
	if (fconfig.Exists("/movie_panel"))
	{
		fconfig.SetPath("/movie_panel");
		wxString sVal;
		bool bVal;
		int iVal;

		//set settings for frame
		VRenderView* vrv = 0;
		if (fconfig.Read("views_cmb", &iVal))
		{
			m_movie_view->m_views_cmb->SetSelection(iVal);
			m_mov_view = iVal;
			vrv = (*GetViewList())[m_mov_view];
		}
		if (fconfig.Read("rot_check", &bVal))
		{
			if (bVal)
				m_movie_view->EnableRot();
			else
				m_movie_view->DisableRot();
		}
		if (fconfig.Read("seq_check", &bVal))
		{
			if (bVal)
				m_movie_view->EnableTime();
			else
				m_movie_view->DisableTime();
		}
		if (fconfig.Read("x_rd", &bVal))
		{
			m_movie_view->m_x_rd->SetValue(bVal);
			if (bVal)
				m_mov_axis = 1;
		}
		if (fconfig.Read("y_rd", &bVal))
		{
			m_movie_view->m_y_rd->SetValue(bVal);
			if (bVal)
				m_mov_axis = 2;
		}
		if (fconfig.Read("angle_end_text", &sVal))
		{
			m_movie_view->m_degree_end->SetValue(sVal);
			m_mov_angle_end = sVal;
		}
		if (fconfig.Read("step_text", &sVal))
		{
			m_movie_view->m_movie_time->SetValue(sVal);
			m_mov_step = sVal;
		}
		if (fconfig.Read("frames_text", &sVal))
		{
			m_movie_view->m_fps_text->SetValue(sVal);
			m_mov_frames = sVal;
		}
		if (fconfig.Read("frame_chk", &bVal))
		{
			m_movie_view->m_frame_chk->SetValue(bVal);
			if (vrv)
			{
				if (bVal)
					vrv->EnableFrame();
				else
					vrv->DisableFrame();
			}
		}
		long frame_x, frame_y, frame_w, frame_h;
		bool b_x, b_y, b_w, b_h;
		b_x = b_y = b_w = b_h = false;
		if (fconfig.Read("center_x_text", &sVal) && sVal!="")
		{
			m_movie_view->m_center_x_text->SetValue(sVal);
			sVal.ToLong(&frame_x);
			b_x = true;
		}
		if (fconfig.Read("center_y_text", &sVal) && sVal!="")
		{
			m_movie_view->m_center_y_text->SetValue(sVal);
			sVal.ToLong(&frame_y);
			b_y = true;
		}
		if (fconfig.Read("width_text", &sVal) && sVal!="")
		{
			m_movie_view->m_width_text->SetValue(sVal);
			sVal.ToLong(&frame_w);
			b_w = true;
		}
		if (fconfig.Read("height_text", &sVal) && sVal!="")
		{
			m_movie_view->m_height_text->SetValue(sVal);
			sVal.ToLong(&frame_h);
			b_h = true;
		}
		if (vrv && b_x && b_y && b_w && b_h)
		{
			vrv->SetFrame(int(frame_x-frame_w/2.0+0.5),
				int(frame_y-frame_h/2.0+0.5),
				frame_w, frame_h);
		}
		if (fconfig.Read("time_start_text", &sVal))
			m_movie_view->m_time_start_text->SetValue(sVal);
		if (fconfig.Read("time_end_text", &sVal))
			m_movie_view->m_time_end_text->SetValue(sVal);
	}

	//brushtool diag
	if (fconfig.Exists("/brush_diag"))
	{
		fconfig.SetPath("/brush_diag");
		double dval;

		if (fconfig.Read("ca_min", &dval))
			m_brush_tool_dlg->SetDftCAMin(dval);
		if (fconfig.Read("ca_max", &dval))
			m_brush_tool_dlg->SetDftCAMax(dval);
		if (fconfig.Read("ca_thresh", &dval))
			m_brush_tool_dlg->SetDftCAThresh(dval);
		if (fconfig.Read("nr_thresh", &dval))
		{
			m_brush_tool_dlg->SetDftNRThresh(dval);
			m_noise_cancelling_dlg->SetDftThresh(dval);
		}
		if (fconfig.Read("nr_size", &dval))
		{
			m_brush_tool_dlg->SetDftNRSize(dval);
			m_noise_cancelling_dlg->SetDftSize(dval);
		}
	}

	wxString expstate;
	if (fconfig.Exists("/tree_panel"))
	{
		fconfig.SetPath("/tree_panel");
		fconfig.Read("exp_states", &expstate);
	}

	//ui layout
	if (fconfig.Exists("/ui_layout"))
	{
		fconfig.SetPath("/ui_layout");
		bool bVal;

		if (fconfig.Read("ui_main_tb", &bVal))
		{
			if (bVal)
			{
				m_aui_mgr.GetPane(m_main_tb).Show();
				bool fl;
				if (fconfig.Read("ui_main_tb_float", &fl))
				{
					if (fl)
						m_aui_mgr.GetPane(m_main_tb).Float();
					else
						m_aui_mgr.GetPane(m_main_tb).Dock();
				}
			}
			else
			{
				if (m_aui_mgr.GetPane(m_main_tb).IsOk())
					m_aui_mgr.GetPane(m_main_tb).Hide();
			}
		}
		if (fconfig.Read("ui_tree_view", &bVal))
		{
			if (bVal)
			{
				m_aui_mgr.GetPane(m_tree_panel).Show();
				m_tb_menu_ui->Check(ID_UITreeView, true);
				bool fl;
				if (fconfig.Read("ui_tree_view_float", &fl))
				{
					if (fl)
						m_aui_mgr.GetPane(m_tree_panel).Float();
					else
						m_aui_mgr.GetPane(m_tree_panel).Dock();
				}
			}
			else
			{
				if (m_aui_mgr.GetPane(m_tree_panel).IsOk())
					m_aui_mgr.GetPane(m_tree_panel).Hide();
				m_tb_menu_ui->Check(ID_UITreeView, false);
			}
		}
		if (fconfig.Read("ui_measure_view", &bVal))
		{
/*			if (bVal)
			{
				m_aui_mgr.GetPane(m_measure_dlg).Show();
				m_tb_menu_ui->Check(ID_UIMeasureView, true);
				bool fl;
				if (fconfig.Read("ui_measure_view_float", &fl))
				{
					if (fl)
						m_aui_mgr.GetPane(m_measure_dlg).Float();
					else
						m_aui_mgr.GetPane(m_measure_dlg).Dock();
				}
			}
			else
*/			{
				if (m_aui_mgr.GetPane(m_measure_dlg).IsOk())
					m_aui_mgr.GetPane(m_measure_dlg).Hide();
				m_tb_menu_ui->Check(ID_UIMeasureView, false);
			}
		}
		if (fconfig.Read("ui_movie_view", &bVal))
		{
			if (bVal)
			{
				m_aui_mgr.GetPane(m_movie_view).Show();
				m_tb_menu_ui->Check(ID_UIMovieView, true);
				bool fl;
				if (fconfig.Read("ui_movie_view_float", &fl))
				{
					if (fl)
						m_aui_mgr.GetPane(m_movie_view).Float();
					else
						m_aui_mgr.GetPane(m_movie_view).Dock();
				}
			}
			else
			{
				if (m_aui_mgr.GetPane(m_movie_view).IsOk())
					m_aui_mgr.GetPane(m_movie_view).Hide();
				m_tb_menu_ui->Check(ID_UIMovieView, false);
			}
		}
		if (fconfig.Read("ui_adjust_view", &bVal))
		{
			if (bVal)
			{
				m_aui_mgr.GetPane(m_adjust_view).Show();
				m_tb_menu_ui->Check(ID_UIAdjView, true);
				bool fl;
				if (fconfig.Read("ui_adjust_view_float", &fl))
				{
					if (fl)
						m_aui_mgr.GetPane(m_adjust_view).Float();
					else
						m_aui_mgr.GetPane(m_adjust_view).Dock();
				}
			}
			else
			{
				if (m_aui_mgr.GetPane(m_adjust_view).IsOk())
					m_aui_mgr.GetPane(m_adjust_view).Hide();
				m_tb_menu_ui->Check(ID_UIAdjView, false);
			}
		}
		if (fconfig.Read("ui_clip_view", &bVal))
		{
			if (bVal)
			{
				m_aui_mgr.GetPane(m_clip_view).Show();
				m_tb_menu_ui->Check(ID_UIClipView, true);
				bool fl;
				if (fconfig.Read("ui_clip_view_float", &fl))
				{
					if (fl)
						m_aui_mgr.GetPane(m_clip_view).Float();
					else
						m_aui_mgr.GetPane(m_clip_view).Dock();
				}
			}
			else
			{
				if (m_aui_mgr.GetPane(m_clip_view).IsOk())
					m_aui_mgr.GetPane(m_clip_view).Hide();
				m_tb_menu_ui->Check(ID_UIClipView, false);
			}
		}
		if (fconfig.Read("ui_prop_view", &bVal))
		{
			if (bVal)
			{
				m_aui_mgr.GetPane(m_prop_panel).Show();
				m_tb_menu_ui->Check(ID_UIPropView, true);
				bool fl;
				if (fconfig.Read("ui_prop_view_float", &fl))
				{
					if (fl)
						m_aui_mgr.GetPane(m_prop_panel).Float();
					else
						m_aui_mgr.GetPane(m_prop_panel).Dock();
				}
			}
			else
			{
				if (m_aui_mgr.GetPane(m_prop_panel).IsOk())
					m_aui_mgr.GetPane(m_prop_panel).Hide();
				m_tb_menu_ui->Check(ID_UIPropView, false);
			}
		}

		m_aui_mgr.Update();
	}

	//interpolator
	if (fconfig.Exists("/interpolator"))
	{
		wxString str;
		wxString sVal;
		double dVal;

		fconfig.SetPath("/interpolator");
		m_interpolator.Clear();
		if (fconfig.Read("max_id", &iVal))
			Interpolator::m_id = iVal;
		vector<FlKeyGroup*>* key_list = m_interpolator.GetKeyList();
		int group_num = fconfig.Read("num", 0l);
		for (i=0; i<group_num; i++)
		{
			str = wxString::Format("/interpolator/%d", i);
			if (fconfig.Exists(str))
			{
				fconfig.SetPath(str);
				FlKeyGroup* key_group = new FlKeyGroup;
				if (fconfig.Read("id", &iVal))
					key_group->id = iVal;
				if (fconfig.Read("t", &dVal))
					key_group->t = dVal;
				if (fconfig.Read("type", &iVal))
					key_group->type = iVal;
				if (fconfig.Read("desc", &sVal))
					key_group->desc = sVal.ToStdString();
				str = wxString::Format("/interpolator/%d/keys", i);
				if (fconfig.Exists(str))
				{
					fconfig.SetPath(str);
					int key_num = fconfig.Read("num", 0l);
					for (j=0; j<key_num; j++)
					{
						str = wxString::Format("/interpolator/%d/keys/%d", i, j);
						if (fconfig.Exists(str))
						{
							fconfig.SetPath(str);
							int key_type;
							if (fconfig.Read("type", &key_type))
							{
								KeyCode code;
								if (fconfig.Read("l0", &iVal))
									code.l0 = iVal;
								if (fconfig.Read("l0_name", &sVal))
									code.l0_name = sVal.ToStdString();
								if (fconfig.Read("l1", &iVal))
									code.l1 = iVal;
								if (fconfig.Read("l1_name", &sVal))
									code.l1_name = sVal.ToStdString();
								if (fconfig.Read("l2", &iVal))
									code.l2 = iVal;
								if (fconfig.Read("l2_name", &sVal))
									code.l2_name = sVal.ToStdString();
								switch (key_type)
								{
								case FLKEY_TYPE_DOUBLE:
									{
										if (fconfig.Read("val", &dVal))
										{
											FlKeyDouble* key = new FlKeyDouble(code, dVal);
											key_group->keys.push_back(key);
										}
									}
									break;
								case FLKEY_TYPE_QUATER:
									{
										if (fconfig.Read("val", &sVal))
										{
											double x, y, z, w;
											if (SSCANF(sVal.c_str(), "%lf%lf%lf%lf",
												&x, &y, &z, &w))
											{
												Quaternion qval = Quaternion(x, y, z, w);
												FlKeyQuaternion* key = new FlKeyQuaternion(code, qval);
												key_group->keys.push_back(key);
											}
										}
									}
									break;
								case FLKEY_TYPE_BOOLEAN:
									{
										if (fconfig.Read("val", &bVal))
										{
											FlKeyBoolean* key = new FlKeyBoolean(code, bVal);
											key_group->keys.push_back(key);
										}
									}
									break;
								}
							}
						}
					}
				}
				key_list->push_back(key_group);
			}
		}
		m_recorder_dlg->UpdateList();
	}

	if (m_cur_sel_type != -1)
	{
		switch (m_cur_sel_type)
		{
		case 2:  //volume
			if (m_data_mgr.GetVolumeData(m_cur_sel_vol))
				UpdateTree(m_data_mgr.GetVolumeData(m_cur_sel_vol)->GetName(), 2);
			else
				UpdateTree();
			break;
		case 3:  //mesh
			if (m_data_mgr.GetMeshData(m_cur_sel_mesh))
				UpdateTree(m_data_mgr.GetMeshData(m_cur_sel_mesh)->GetName(), 3);
			else
				UpdateTree();
			break;
		default:
			UpdateTree();
		}
	}
	else if (m_cur_sel_vol != -1)
	{
		if (m_data_mgr.GetVolumeData(m_cur_sel_vol))
			UpdateTree(m_data_mgr.GetVolumeData(m_cur_sel_vol)->GetName(), 2);
		else
			UpdateTree();
	}
	else
		UpdateTree();

	if (!expstate.IsEmpty())
		m_tree_panel->ImportExpState(expstate.ToStdString());
	RefreshVRenderViews();

	if (m_movie_view)
		m_movie_view->SetView(0);
	delete prg_diag;
    
    //Thaw();
    SetEvtHandlerEnabled(true);
}

void VRenderFrame::OnSettings(wxCommandEvent& WXUNUSED(event))
{
	m_aui_mgr.GetPane(m_setting_dlg).Show();
	m_aui_mgr.GetPane(m_setting_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnPaintTool(wxCommandEvent& WXUNUSED(event))
{
	ShowPaintTool();
}

void VRenderFrame::ShowPaintTool()
{
	m_aui_mgr.GetPane(m_brush_tool_dlg).Show();
	m_aui_mgr.GetPane(m_brush_tool_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnInfoDlg(wxCommandEvent& WXUNUSED(event))
{
	ShowInfoDlg();
}

void VRenderFrame::ShowInfoDlg()
{
	m_aui_mgr.GetPane(m_anno_view).Show();
	m_aui_mgr.GetPane(m_anno_view).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnNoiseCancelling(wxCommandEvent& WXUNUSED(event))
{
	ShowNoiseCancellingDlg();
}

void VRenderFrame::ShowNoiseCancellingDlg()
{
	m_aui_mgr.GetPane(m_noise_cancelling_dlg).Show();
	m_aui_mgr.GetPane(m_noise_cancelling_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnCounting(wxCommandEvent& WXUNUSED(event))
{
	ShowCountingDlg();
}

void VRenderFrame::ShowCountingDlg()
{
	m_aui_mgr.GetPane(m_counting_dlg).Show();
	m_aui_mgr.GetPane(m_counting_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnColocalization(wxCommandEvent& WXUNUSED(event))
{
	ShowColocalizationDlg();
}

void VRenderFrame::ShowColocalizationDlg()
{
	m_aui_mgr.GetPane(m_colocalization_dlg).Show();
	m_aui_mgr.GetPane(m_colocalization_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnConvert(wxCommandEvent& WXUNUSED(event))
{
	ShowConvertDlg();
}

void VRenderFrame::ShowConvertDlg()
{
	m_aui_mgr.GetPane(m_convert_dlg).Show();
	m_aui_mgr.GetPane(m_convert_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnTrace(wxCommandEvent& WXUNUSED(event))
{
	ShowTraceDlg();
}

void VRenderFrame::ShowTraceDlg()
{
	m_aui_mgr.GetPane(m_trace_dlg).Show();
	m_aui_mgr.GetPane(m_trace_dlg).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::OnMeasure(wxCommandEvent& WXUNUSED(event))
{
	ShowMeasureDlg();
}

void VRenderFrame::ShowMeasureDlg()
{
	m_tb_menu_ui->Check(ID_UIMeasureView, true);
	m_aui_mgr.GetPane(m_measure_dlg).Show();
	m_aui_mgr.Update();
}

void VRenderFrame::OnRecorder(wxCommandEvent& WXUNUSED(event))
{
	ShowRecorderDlg();
}

void VRenderFrame::ShowRecorderDlg()
{
	m_tb_menu_ui->Check(ID_UIMovieView, true);
	m_aui_mgr.GetPane(m_movie_view).Show();
	//m_aui_mgr.GetPane(m_movie_view).Float();
	m_aui_mgr.Update();
}

void VRenderFrame::SetTextureUndos()
{
	if (m_setting_dlg)
		Texture::mask_undo_num_ = (size_t)(m_setting_dlg->GetPaintHistDepth());
}

void VRenderFrame::SetTextureRendererSettings()
{
	if (!m_setting_dlg)
		return;

	TextureRenderer::set_mem_swap(m_setting_dlg->GetMemSwap());
	bool use_mem_limit = true;

	//check amount of graphic memory
	if (m_gpu_max_mem <= 0.0)
	{
		GLenum error = glGetError();
		GLint mem_info[4] = {0, 0, 0, 0};
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, mem_info);
		error = glGetError();
		if (error == GL_INVALID_ENUM)
		{
			glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, mem_info);
			error = glGetError();
		}
		if (error != GL_INVALID_ENUM)
			m_gpu_max_mem = mem_info[0]/1024.0;
	}
	double mem_size = m_gpu_max_mem > m_setting_dlg->GetGraphicsMem() ? m_setting_dlg->GetGraphicsMem() : m_gpu_max_mem;

	//reserve a memory area for 2D textures
	//(TextureRenderer takes no account of 2D textures in calculating allocated memory size)
	if (mem_size > 1024.0) mem_size -= 300.0;
	else mem_size *= 0.7;

	double mem_delta = mem_size - TextureRenderer::get_mem_limit();
	double prev_available_mem = TextureRenderer::get_available_mem();

	TextureRenderer::set_use_mem_limit(use_mem_limit);
	TextureRenderer::set_mem_limit(mem_size);
	TextureRenderer::set_available_mem(mem_delta + prev_available_mem);
	TextureRenderer::set_large_data_size(m_setting_dlg->GetLargeDataSize());
	TextureRenderer::set_force_brick_size(m_setting_dlg->GetForceBrickSize());
	TextureRenderer::set_up_time(m_setting_dlg->GetResponseTime());
	TextureRenderer::set_update_order(m_setting_dlg->GetUpdateOrder());
}

void VRenderFrame::OnFacebook(wxCommandEvent& WXUNUSED(event))
{
	::wxLaunchDefaultBrowser("http://www.facebook.com/fluorender");
}

void VRenderFrame::OnTwitter(wxCommandEvent& WXUNUSED(event))
{
	::wxLaunchDefaultBrowser("http://twitter.com/FluoRender");
}

void VRenderFrame::OnShowHideUI(wxCommandEvent& WXUNUSED(event))
{
	ToggleAllTools();
}

void VRenderFrame::OnShowHideToolbar(wxCommandEvent& WXUNUSED(event))
{
	if (m_aui_mgr.GetPane(m_main_tb).IsShown()) {
		m_aui_mgr.GetPane(m_main_tb).Hide();
		m_top_window->Check(ID_ShowHideToolbar,false);
	} else {
		m_aui_mgr.GetPane(m_main_tb).Show();
		m_top_window->Check(ID_ShowHideToolbar,true);
	}
	m_aui_mgr.Update();
}

void VRenderFrame::OnShowHideView(wxCommandEvent &event)
{
	int id = event.GetId();

	switch (id)
	{
	case ID_UITreeView:
		//scene view
		if (m_aui_mgr.GetPane(m_tree_panel).IsShown())
		{
			m_aui_mgr.GetPane(m_tree_panel).Hide();
			m_tb_menu_ui->Check(ID_UITreeView, false);
		}
		else
		{
			m_aui_mgr.GetPane(m_tree_panel).Show();
			m_tb_menu_ui->Check(ID_UITreeView, true);
		}
		break;
	case ID_UIMovieView:
		//measurement view
		if (m_aui_mgr.GetPane(m_movie_view).IsShown())
		{
			m_aui_mgr.GetPane(m_movie_view).Hide();
			m_tb_menu_ui->Check(ID_UIMovieView, false);
		}
		else
		{
			m_aui_mgr.GetPane(m_movie_view).Show();
			m_tb_menu_ui->Check(ID_UIMovieView, true);
		}
		break;
	case ID_UIMeasureView:
		//measurement view
		if (m_aui_mgr.GetPane(m_measure_dlg).IsShown())
		{
			m_aui_mgr.GetPane(m_measure_dlg).Hide();
			m_tb_menu_ui->Check(ID_UIMeasureView, false);
		}
		else
		{
			m_aui_mgr.GetPane(m_measure_dlg).Show();
			m_tb_menu_ui->Check(ID_UIMeasureView, true);
		}
		break;
	case ID_UIAdjView:
		//adjust view
		if (m_aui_mgr.GetPane(m_adjust_view).IsShown())
		{
			m_aui_mgr.GetPane(m_adjust_view).Hide();
			m_tb_menu_ui->Check(ID_UIAdjView, false);
		}
		else
		{
			m_aui_mgr.GetPane(m_adjust_view).Show();
			m_tb_menu_ui->Check(ID_UIAdjView, true);
		}
		break;
	case ID_UIClipView:
		//clipping view
		if (m_aui_mgr.GetPane(m_clip_view).IsShown())
		{
			m_aui_mgr.GetPane(m_clip_view).Hide();
			m_tb_menu_ui->Check(ID_UIClipView, false);
		}
		else
		{
			m_aui_mgr.GetPane(m_clip_view).Show();
			m_tb_menu_ui->Check(ID_UIClipView, true);
		}
		break;
	case ID_UIPropView:
		//properties
		if (m_aui_mgr.GetPane(m_prop_panel).IsShown())
		{
			m_aui_mgr.GetPane(m_prop_panel).Hide();
			m_tb_menu_ui->Check(ID_UIPropView, false);
		}
		else
		{
			m_aui_mgr.GetPane(m_prop_panel).Show();
			m_tb_menu_ui->Check(ID_UIPropView, true);
		}
		break;
	}

	m_aui_mgr.Update();
}

//panes
void VRenderFrame::OnPaneClose(wxAuiManagerEvent& event)
{
	wxWindow* wnd = event.pane->window;
	wxString name = wnd->GetName();

	if (name == "TreePanel")
		m_tb_menu_ui->Check(ID_UITreeView, false);
	else if (name == "MeasureDlg")
		m_tb_menu_ui->Check(ID_UIMeasureView, false);
	else if (name == "VMovieView")
		m_tb_menu_ui->Check(ID_UIMovieView, false);
	else if (name == "PropPanel")
		m_tb_menu_ui->Check(ID_UIPropView, false);
	else if (name == "AdjustView")
		m_tb_menu_ui->Check(ID_UIAdjView, false);
	else if (name == "ClippingView")
		m_tb_menu_ui->Check(ID_UIClipView, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void VRenderFrame::OnCreateCube(wxCommandEvent& WXUNUSED(event))
{
}

void VRenderFrame::OnCreateSphere(wxCommandEvent& WXUNUSED(event))
{
}

void VRenderFrame::OnCreateCone(wxCommandEvent& WXUNUSED(event))
{
}

void VRenderFrame::OnDraw(wxPaintEvent& event)
{
	//wxPaintDC dc(this);

	//wxRect windowRect(wxPoint(0, 0), GetClientSize());
	//dc.SetBrush(wxBrush(*wxGREEN, wxSOLID));
	//dc.DrawRectangle(windowRect);
}

void VRenderFrame::OnKeyDown(wxKeyEvent& event)
{
	event.Skip();
}

