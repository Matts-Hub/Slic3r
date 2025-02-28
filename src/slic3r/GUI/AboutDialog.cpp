#include "AboutDialog.hpp"
#include "I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "GUI_App.hpp"
#include "wxExtensions.hpp"

namespace Slic3r { 
namespace GUI {

AboutDialogLogo::AboutDialogLogo(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
{
    this->SetBackgroundColour(*wxWHITE);
    this->logo = wxBitmap(from_u8(Slic3r::var("Slic3r_192px.png")), wxBITMAP_TYPE_PNG);
    this->SetMinSize(this->logo.GetSize());
    
    this->Bind(wxEVT_PAINT, &AboutDialogLogo::onRepaint, this);
}

void AboutDialogLogo::onRepaint(wxEvent &event)
{
    wxPaintDC dc(this);
    dc.SetBackgroundMode(wxTRANSPARENT);

    wxSize size = this->GetSize();
    int logo_w = this->logo.GetWidth();
    int logo_h = this->logo.GetHeight();
    dc.DrawBitmap(this->logo, (size.GetWidth() - logo_w)/2, (size.GetHeight() - logo_h)/2, true);

    event.Skip();
}


// -----------------------------------------
// CopyrightsDialog
// -----------------------------------------
CopyrightsDialog::CopyrightsDialog()
    : DPIDialog(NULL, wxID_ANY, wxString::Format("%s - %s", SLIC3R_APP_NAME, _(L("Portions copyright"))), 
                wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    this->SetFont(wxGetApp().normal_font());
	this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

	auto sizer = new wxBoxSizer(wxVERTICAL);
    
    fill_entries();

    m_html = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, 
                              wxSize(40 * em_unit(), 20 * em_unit()), wxHW_SCROLLBAR_AUTO);

    wxFont font = GetFont();
    const int fs = font.GetPointSize();
    const int fs2 = static_cast<int>(1.2f*fs);
    int size[] = { fs, fs, fs, fs, fs2, fs2, fs2 };

    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), size);
    m_html->SetBorders(2);        
    m_html->SetPage(get_html_text());

    sizer->Add(m_html, 1, wxEXPAND | wxALL, 15);
    m_html->Bind(wxEVT_HTML_LINK_CLICKED, &CopyrightsDialog::onLinkClicked, this);

    wxStdDialogButtonSizer* buttons = this->CreateStdDialogButtonSizer(wxCLOSE);

    this->SetEscapeId(wxID_CLOSE);
    this->Bind(wxEVT_BUTTON, &CopyrightsDialog::onCloseDialog, this, wxID_CLOSE);
    sizer->Add(buttons, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 3);

    SetSizer(sizer);
    sizer->SetSizeHints(this);
    
}

void CopyrightsDialog::fill_entries()
{
    m_entries = {
        { "wxWidgets"       , "2019 wxWidgets"                              , "https://www.wxwidgets.org/" },
        { "OpenGL"          , "1997-2019 The Khronos� Group Inc"            , "https://www.opengl.org/" },
        { "GNU gettext"     , "1998, 2019 Free Software Foundation, Inc."   , "https://www.gnu.org/software/gettext/" },
        { "PoEdit"          , "2019 V�clav Slav�k"                          , "https://poedit.net/" },
        { "ImGUI"           , "2014-2019 Omar Cornut"                       , "https://github.com/ocornut/imgui" },
        { "Eigen"           , ""                                            , "http://eigen.tuxfamily.org" },
        { "ADMesh"          , "1995, 1996  Anthony D. Martin; "
                              "2015, ADMesh contributors"                   , "https://admesh.readthedocs.io/en/latest/" },
        { "Anti-Grain Geometry"
                            , "2002-2005 Maxim Shemanarev (McSeem)"         , "http://antigrain.com" },
        { "Boost"           , "1998-2005 Beman Dawes, David Abrahams; "
                              "2004 - 2007 Rene Rivera"                     , "https://www.boost.org/" },
        { "Clipper"         , "2010-2015 Angus Johnson "                    , "http://www.angusj.com " },
        { "GLEW (The OpenGL Extension Wrangler Library)", 
                              "2002 - 2007, Milan Ikits; "
                              "2002 - 2007, Marcelo E.Magallon; "
                              "2002, Lev Povalahev"                         , "http://glew.sourceforge.net/" },
        { "Libigl"          , "2013 Alec Jacobson and others"               , "https://libigl.github.io/" },
        { "Poly2Tri"        , "2009-2018, Poly2Tri Contributors"            , "https://github.com/jhasse/poly2tri" },
        { "PolyPartition"   , "2011 Ivan Fratric"                           , "https://github.com/ivanfratric/polypartition" },
        { "Qhull"           , "1993-2015 C.B.Barber Arlington and "
                              "University of Minnesota"                     , "http://qhull.org/" },
        { "SemVer"          , "2015-2017 Tomas Aparicio"                    , "https://semver.org/" },
        { "Nanosvg"         , "2013-14 Mikko Mononen"                       , "https://github.com/memononen/nanosvg" },
        { "Miniz"           , "2013-2014 RAD Game Tools and Valve Software; "
                              "2010-2014 Rich Geldreich and Tenacious Software LLC"
                                                                            , "https://github.com/richgel999/miniz" },
        { "Expat"           , "1998-2000 Thai Open Source Software Center Ltd and Clark Cooper"
                              "2001-2016 Expat maintainers"                 , "http://www.libexpat.org/" },
        { "AVRDUDE"         , "2018  Free Software Foundation, Inc."        , "http://savannah.nongnu.org/projects/avrdude" },
        { "Shinyprofiler"   , "2007-2010 Aidin Abedi"                       , "http://code.google.com/p/shinyprofiler/" },
        { "Icons for STL and GCODE files."
                            , "Akira Yasuda"                                , "http://3dp0.com/icons-for-stl-and-gcode/" }
    };
}

wxString CopyrightsDialog::get_html_text()
{
    wxColour bgr_clr = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

    const auto text_clr = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const auto text_clr_str = wxString::Format(wxT("#%02X%02X%02X"), text_clr.Red(), text_clr.Green(), text_clr.Blue());
    const auto bgr_clr_str = wxString::Format(wxT("#%02X%02X%02X"), bgr_clr.Red(), bgr_clr.Green(), bgr_clr.Blue());

    const wxString copyright_str = _(L("Copyright")) + "&copy; ";
    // TRN "Slic3r _is licensed under the_ License"
    const wxString header_str = _(L("License agreements of all following programs (libraries) are part of application license agreement"));

    wxString text = wxString::Format(
        "<html>"
            "<body bgcolor= %s link= %s>"
            "<font color=%s>"
                "<font size=\"5\">%s.</font>"
                "<br /><br />"
                "<font size=\"3\">"
        , bgr_clr_str, text_clr_str
        , text_clr_str
        , header_str);

    for (auto& entry : m_entries) {
        text += wxString::Format(
                    "<a href=\"%s\">%s</a><br/>"
                    , entry.link, entry.lib_name);

        if (!entry.copyright.empty())
            text += wxString::Format(
                    "%s %s"
                    "<br/><br/>"
                    , copyright_str, entry.copyright);
    }

    text += wxString(
                "</font>"
            "</font>"
            "</body>"
        "</html>");

    return text;
}

void CopyrightsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    const wxFont& font = GetFont();
    const int fs = font.GetPointSize();
    const int fs2 = static_cast<int>(1.2f*fs);
    int font_size[] = { fs, fs, fs, fs, fs2, fs2, fs2 };

    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), font_size);

    const int& em = em_unit();

    msw_buttons_rescale(this, em, { wxID_CLOSE });

    const wxSize& size = wxSize(40 * em, 20 * em);

    m_html->SetMinSize(size);
    m_html->Refresh();

    SetMinSize(size);
    Fit();

    Refresh();
}

void CopyrightsDialog::onLinkClicked(wxHtmlLinkEvent &event)
{
    wxLaunchDefaultBrowser(event.GetLinkInfo().GetHref());
    event.Skip(false);
}

void CopyrightsDialog::onCloseDialog(wxEvent &)
{
     this->EndModal(wxID_CLOSE);
}

AboutDialog::AboutDialog()
    : DPIDialog(NULL, wxID_ANY, wxString::Format(_(L("About %s")), SLIC3R_APP_NAME), wxDefaultPosition, 
                wxDefaultSize, /*wxCAPTION*/wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetFont(wxGetApp().normal_font());

	wxColour bgr_clr = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	SetBackgroundColour(bgr_clr);
    wxBoxSizer* hsizer = new wxBoxSizer(wxHORIZONTAL);

	auto main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(hsizer, 0, wxEXPAND | wxALL, 20);

    // logo
    m_logo_bitmap = ScalableBitmap(this, "Slic3r_192px.png", 192);
    m_logo = new wxStaticBitmap(this, wxID_ANY, m_logo_bitmap.bmp());
	hsizer->Add(m_logo, 1, wxALIGN_CENTER_VERTICAL);
    
    wxBoxSizer* vsizer = new wxBoxSizer(wxVERTICAL); 	
    hsizer->Add(vsizer, 2, wxEXPAND|wxLEFT, 20);

    // title
    {
        wxStaticText* title = new wxStaticText(this, wxID_ANY, SLIC3R_APP_NAME, wxDefaultPosition, wxDefaultSize);
        wxFont title_font = GUI::wxGetApp().bold_font();
        title_font.SetFamily(wxFONTFAMILY_ROMAN);
        title_font.SetPointSize(24);
        title->SetFont(title_font);
        vsizer->Add(title, 0, wxALIGN_LEFT | wxTOP, 10);
    }
    
    // version
    {
        auto version_string = _(L("Version"))+ " " + std::string(SLIC3R_VERSION);
        wxStaticText* version = new wxStaticText(this, wxID_ANY, version_string.c_str(), wxDefaultPosition, wxDefaultSize);
        wxFont version_font = GetFont();
        #ifdef __WXMSW__
        version_font.SetPointSize(version_font.GetPointSize()-1);
        #else
            version_font.SetPointSize(11);
        #endif
        version->SetFont(version_font);
        vsizer->Add(version, 0, wxALIGN_LEFT | wxBOTTOM, 10);
    }
    
    // text
    m_html = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_AUTO/*NEVER*/);
    {
        m_html->SetMinSize(wxSize(-1, 16 * wxGetApp().em_unit()));
        wxFont font = GetFont();
        const auto text_clr = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		auto text_clr_str = wxString::Format(wxT("#%02X%02X%02X"), text_clr.Red(), text_clr.Green(), text_clr.Blue());
		auto bgr_clr_str = wxString::Format(wxT("#%02X%02X%02X"), bgr_clr.Red(), bgr_clr.Green(), bgr_clr.Blue());

		const int fs = font.GetPointSize()-1;
        int size[] = {fs,fs,fs,fs,fs,fs,fs};
        m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), size);
        m_html->SetBorders(2);
        const wxString copyright_str = _(L("Copyright"));
        // TRN "Slic3r _is licensed under the_ License"
        const wxString is_lecensed_str = _(L("is licensed under the"));
        const wxString license_str = _(L("GNU Affero General Public License, version 3"));
        const wxString based_on_str = _(L("Slic3r++ is based on PrusaSlicer which is based on Slic3r by Alessandro Ranellucci and the RepRap community."));
        const wxString contributors_str = _(L("Contributions by Henrik Brix Andersen, Nicolas Dandrimont, Mark Hindess, Petr Ledvina, Joseph Lenox, Y. Sapir, Mike Sheldrake, Vojtech Bubnik, Durand R�mi and numerous others."));
		const auto text = wxString::Format(
            "<html>"
            "<body bgcolor= %s link= %s>"
            "<font color=%s>"
            "Copyright &copy; 2018-2019 Durand R�mi. <br />"
            "Copyright &copy; 2016-2019 Prusa Research. <br />"
            "Copyright &copy; 2011-2017 Alessandro Ranellucci. <br />"
            "<a href=\"http://slic3r.org/\">Slic3r</a> is licensed under the "
            "<a href=\"http://www.gnu.org/licenses/agpl-3.0.html\">GNU Affero General Public License, version 3</a>."
            "<br /><br />"
            "Contributions by Henrik Brix Andersen, Nicolas Dandrimont, Mark Hindess, Petr Ledvina, Joseph Lenox, Y. Sapir, Mike Sheldrake, Vojtech Bubnik, Durand R�mi and numerous others. "
            "Manual by Gary Hodgson. Inspired by the RepRap community. <br />"
            "Slic3r logo designed by Corey Daniels, <a href=\"http://www.famfamfam.com/lab/icons/silk/\">Silk Icon Set</a> designed by Mark James. "
            "</font>"
            "</body>"
            "</html>", bgr_clr_str, text_clr_str, text_clr_str,
            copyright_str, copyright_str,
            is_lecensed_str,
            license_str,
            based_on_str,
            contributors_str);
        m_html->SetPage(text);
        vsizer->Add(m_html, 1, wxEXPAND | wxBOTTOM, 10);
        m_html->Bind(wxEVT_HTML_LINK_CLICKED, &AboutDialog::onLinkClicked, this);
    }


    wxStdDialogButtonSizer* buttons = this->CreateStdDialogButtonSizer(wxCLOSE);

    m_copy_rights_btn_id = NewControlId();
    auto copy_rights_btn = new wxButton(this, m_copy_rights_btn_id, _(L("Portions copyright"))+dots);
    buttons->Insert(0, copy_rights_btn, 0, wxLEFT, 5);
    copy_rights_btn->Bind(wxEVT_BUTTON, &AboutDialog::onCopyrightBtn, this);
    
    this->SetEscapeId(wxID_CLOSE);
    this->Bind(wxEVT_BUTTON, &AboutDialog::onCloseDialog, this, wxID_CLOSE);
    vsizer->Add(buttons, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 3);

	SetSizer(main_sizer);
	main_sizer->SetSizeHints(this);
}

void AboutDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    m_logo_bitmap.msw_rescale();
    m_logo->SetBitmap(m_logo_bitmap.bmp());

    const wxFont& font = GetFont();
    const int fs = font.GetPointSize() - 1;
    int font_size[] = { fs, fs, fs, fs, fs, fs, fs };
    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), font_size);

    const int& em = em_unit();

    msw_buttons_rescale(this, em, { wxID_CLOSE, m_copy_rights_btn_id });

    m_html->SetMinSize(wxSize(-1, 16 * em));
    m_html->Refresh();

    const wxSize& size = wxSize(65 * em, 30 * em);

    SetMinSize(size);
    Fit();

    Refresh();
}

void AboutDialog::onLinkClicked(wxHtmlLinkEvent &event)
{
    wxLaunchDefaultBrowser(event.GetLinkInfo().GetHref());
    event.Skip(false);
}

void AboutDialog::onCloseDialog(wxEvent &)
{
    this->EndModal(wxID_CLOSE);
}

void AboutDialog::onCopyrightBtn(wxEvent &)
{
    CopyrightsDialog dlg;
    dlg.ShowModal();
}

} // namespace GUI
} // namespace Slic3r
