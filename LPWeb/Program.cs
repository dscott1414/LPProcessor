/*
	Program.cs - ASP.NET Core host bootstrap for the LPWeb Razor site

	Overview:
		Stock CreateDefaultBuilder / UseStartup<Startup> entry. Builds
		and runs the Kestrel/IIS host that serves the LPWeb Razor Pages.

	Pipeline position:
		Web front-end process entry; not on the C++ parser path.

	Key entry points:
		- Main() - build and run the host
		- CreateHostBuilder() - default host + Startup wiring
*/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace LPWeb
{
    public class Program
    {
        // Build the default host and block until it shuts down.
        public static void Main(string[] args)
        {
            CreateHostBuilder(args).Build().Run();
        }

        // Generic host with web defaults, Startup as the composition root.
        public static IHostBuilder CreateHostBuilder(string[] args) =>
            Host.CreateDefaultBuilder(args)
                .ConfigureWebHostDefaults(webBuilder =>
                {
                    webBuilder.UseStartup<Startup>();
                });
    }
}
