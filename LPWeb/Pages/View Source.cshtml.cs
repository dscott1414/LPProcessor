using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using System.IO;

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
    }
    public class SourceMenuData
    {
        var dirs = from dir in
             Directory.EnumerateDirectories(@"M:\caches\texts")
                   select dir;

    }
}
