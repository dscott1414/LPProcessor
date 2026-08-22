/*
	Error.cshtml.cs - Razor Page model for /Error

	Overview:
		Uncached error page. Surfaces Activity.Current.Id or the HTTP
		trace identifier so a user can quote a request id.

	Pipeline position:
		Wired from Startup.Configure as UseExceptionHandler("/Error")
		in non-Development environments.
*/
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using Microsoft.Extensions.Logging;

namespace LPWeb.Pages
{
    [ResponseCache(Duration = 0, Location = ResponseCacheLocation.None, NoStore = true)]
    public class ErrorModel : PageModel
    {
        public string RequestId { get; set; }

        public bool ShowRequestId => !string.IsNullOrEmpty(RequestId);

        private readonly ILogger<ErrorModel> _logger;

        // DI: logger unused; request id is taken from Activity / TraceIdentifier.
        public ErrorModel(ILogger<ErrorModel> logger)
        {
            _logger = logger;
        }

        // GET /Error — populate RequestId for the view.
        public void OnGet()
        {
            RequestId = Activity.Current?.Id ?? HttpContext.TraceIdentifier;
        }
    }
}
