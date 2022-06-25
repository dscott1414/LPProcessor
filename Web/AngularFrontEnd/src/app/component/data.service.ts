import { Injectable } from '@angular/core';
import { Source } from './source';
import { Observable, throwError } from 'rxjs';
import { catchError, retry } from 'rxjs/operators';
import {HttpClient, HttpHeaders, HttpParams} from '@angular/common/http';


@Injectable({
  providedIn: 'root'
})
export class DataService {

  searchOption: Source = {sourceId: -1, title: "", author: ""};
  configUrl = 'assets/config.json';
  public postsData: Source[] = [];
  public loaded = false;
  constructor(
    private http: HttpClient
  ) {
  }

  getFilteredSourcesList(author: string, title: string): Observable<any> {
    const params = new HttpParams()
      .set('author', author)
      .set('title', title);
    let response = this.http.request<Source[]>('GET', 'api/searchSource', {responseType: 'json', params: params});
    return response;
  }

  getFilteredAuthorsList(author: string, title: string): Observable<any> {
    const params = new HttpParams()
      .set('author', author)
      .set('title', title);
    let response = this.http.request<string[]>('GET', 'api/searchAuthor', {responseType: 'json', params: params});
    return response;
  }

  loadSource(e: any): Observable<any> {
    const loadParams = new HttpParams()
      .set('author', e.author)
      .set('title', e.title)
    return this.http.request<string[]>('GET', 'api/loadSource', {responseType: 'json', params: loadParams});
  }

  loadElements(width:number, height:number, fromWhere:number): Observable<any> {
    const elementParams = new HttpParams()
        .set('width', width)
        .set('height', height)
        .set('fromWhere', fromWhere);
    return this.http.request<string[]>('GET', 'api/loadElements', {responseType: 'json', params: elementParams});
  }

  loadChapters(e: any): Observable<any> {
    return this.http.request<string[]>('GET', 'api/loadChapters', {responseType: 'json'});
  }

}
