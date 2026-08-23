/*
	About.cshtml.cs - Razor Page model for /About

	Overview:
		Empty GET handler; the About page is static markup only.

	Pipeline position:
		LPWeb Razor Pages surface; no parser or DB calls.
*/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;

namespace LPWeb.Pages
{
    public class AboutModel : PageModel
    {
        // GET /About — no model data.
        public void OnGet()
        {
        }
    }
}
