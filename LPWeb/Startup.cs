/*
	Startup.cs - ASP.NET Core DI and middleware pipeline for LPWeb

	Overview:
		Registers Razor Pages and configures the request pipeline:
		dev exception page vs /Error + HSTS, HTTPS redirect, static
		files, routing, authorization, MapRazorPages.

	Pipeline position:
		Web front-end composition root, invoked from Program.CreateHostBuilder.

	Key entry points:
		- Startup() - stash IConfiguration
		- ConfigureServices() - AddRazorPages
		- Configure() - middleware order

	Notes / gotchas:
		UseAuthorization is present but no authentication is configured,
		so authorization is a no-op unless later middleware is added.
*/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.HttpsPolicy;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;

namespace LPWeb
{
    public class Startup
    {
        // Capture the host-provided configuration (appsettings, env, cmdline).
        public Startup(IConfiguration configuration)
        {
            Configuration = configuration;
        }

        public IConfiguration Configuration { get; }

        // This method gets called by the runtime. Use this method to add services to the container.
        public void ConfigureServices(IServiceCollection services)
        {
            services.AddRazorPages();
        }

        // This method gets called by the runtime. Use this method to configure the HTTP request pipeline.
        public void Configure(IApplicationBuilder app, IWebHostEnvironment env)
        {
            if (env.IsDevelopment())
            {
                app.UseDeveloperExceptionPage();
            }
            else
            {
                app.UseExceptionHandler("/Error");
                // The default HSTS value is 30 days. You may want to change this for production scenarios, see https://aka.ms/aspnetcore-hsts.
                app.UseHsts();
            }

            app.UseHttpsRedirection();
            app.UseStaticFiles();

            app.UseRouting();

            app.UseAuthorization();

            app.UseEndpoints(endpoints =>
            {
                endpoints.MapRazorPages();
            });
        }
    }
}
