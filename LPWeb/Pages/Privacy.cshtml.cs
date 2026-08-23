/*
	Privacy.cshtml.cs - Razor Page model for /Privacy

	Overview:
		Stock privacy page. Logger is injected but unused; GET is empty.

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
    public class PrivacyModel : PageModel
    {
        private readonly ILogger<PrivacyModel> _logger;

        // DI: keep ILogger for future diagnostics; currently unused.
        public PrivacyModel(ILogger<PrivacyModel> logger)
        {
            _logger = logger;
        }

        // GET /Privacy — no model data.
        public void OnGet()
        {
        }
    }
}
