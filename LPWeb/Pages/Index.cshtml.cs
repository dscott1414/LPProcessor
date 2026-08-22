/*
	Index.cshtml.cs - Razor Page model for the LPWeb home page

	Overview:
		Stock landing-page model. Logger is injected but unused; GET is empty.

	Pipeline position:
		LPWeb Razor Pages surface; no parser or DB calls.
*/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using Microsoft.Extensions.Logging;

namespace LPWeb.Pages
{
    public class IndexModel : PageModel
    {
        private readonly ILogger<IndexModel> _logger;

        // DI: keep ILogger for future diagnostics; currently unused.
        public IndexModel(ILogger<IndexModel> logger)
        {
            _logger = logger;
        }

        // GET / — no model data.
        public void OnGet()
        {

        }
    }
}
