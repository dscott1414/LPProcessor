using Microsoft.AspNetCore.Mvc.RazorPages;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace LPWeb.Pages
{
    public class View_SourceModel : PageModel
    {
        public void OnGet()
        {
        }
        //private readonly LPWebContext db;
        //public IndexModel(LPWebContext db) => this.db = db;
        //public List<Product> Products { get; set; } = new List<Product>();
        //public Product FeaturedProduct { get; set; }
        //public async Task OnGetAsync()
        //{
        //    Products = await db.Products.ToListAsync();
        //    FeaturedProduct = Products.ElementAt(new Random().Next(Products.Count));
        //}
        public List<String> SourceDirectories { get; set; } = new List<String>();
        public List<string> createMenu()
        {
            var dirs = from dir in
             Directory.EnumerateDirectories(@"M:\caches\texts")
                       select dir;
            return dirs.ToList();
        }
    }
    public class SourceMenuData
    {

    }
}
