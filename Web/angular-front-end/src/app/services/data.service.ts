import {Injectable} from '@angular/core';
import {Source} from '../interfaces/source';
import {Observable, throwError, map} from 'rxjs';
import {catchError, retry, tap} from 'rxjs/operators';
import {HttpClient, HttpHeaders, HttpParams} from '@angular/common/http';
import {WordInfo} from '../interfaces/word-info';

@Injectable({
  providedIn: 'root'
})
export class DataService {

  public searchOption: Source = {sourceId: -1, title: "", author: ""};
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

  generateSourceElements(): Observable<any> {
    return this.http.request<string[]>('GET', 'api/generateSourceElements', {responseType: 'json'});
  }

  loadElements(width: number, height: number, fromWhere: string): Observable<any> {
    const elementParams = new HttpParams()
      .set('width', width)
      .set('height', height)
      .set('fromWhere', fromWhere);
    return this.http.request<string[]>('GET', 'api/loadElements', {responseType: 'json', params: elementParams});
  }

  loadInfoPanel(where: number): Observable<any> {
    const elementParams = new HttpParams()
      .set('where', where);
    return this.http.request<string[]>('GET', 'api/loadInfoPanel', {responseType: 'json', params: elementParams});
  }

  loadChapters(): Observable<any> {
    return this.http.request<string[]>('GET', 'api/loadChapters', {responseType: 'json'});
  }

  loadAgents(): Observable<any> {
    return this.http.request<string[]>('GET', 'api/loadAgents', {responseType: 'json'});
  }

  login(): Observable<any> {
    return this.http.request('GET', 'api/login', {responseType: 'json'});
  }

  setPreference(which: number, value: boolean): Observable<any> {
    return this.http.post('api/setPreference', {"type": which, "value": value});
  }

  findWordInfo(
    beginId: number, endId: number, pageNumber = 0, pageSize = 3): Observable<WordInfo[]> {

    return this.http.get<any>('/api/wordInfo', {
      responseType: 'json',
      params: new HttpParams()
        .set('beginId', beginId.toString())
        .set('endId', endId.toString())
        .set('pageNumber', pageNumber.toString())
        .set('pageSize', pageSize.toString())
    }).pipe(
      map(res => res["response"])
    );
  }

  loadTimelines(pageNumber: number, pageSize: number) {
    return this.http.get<any>('/api/timelineSegments', {
      responseType: 'json',
      params: new HttpParams()
        .set('pageNumber', pageNumber.toString())
        .set('pageSize', pageSize.toString())
    }).pipe(
      map(res => res["response"])
    );
  }

  getSearchStringsList(sourceSearchString: string): Observable<Map<number, string>> {
    return this.http.get<any>('/api/searchStringList', {
      responseType: 'json',
      params: new HttpParams()
        .set('sourceSearchString', sourceSearchString)
    }).pipe(
      map(res => res["response"])
    );
  }

  findParagraphStart(somewhereInParagraph: string): Observable<string> {
    return this.http.get<any>('/api/findParagraphStart', {
      responseType: 'json',
      params: new HttpParams()
        .set('somewhereInParagraph', somewhereInParagraph)
    }).pipe(
      map(res => res["response"])
    );
  }

  getElementType(elementId: string): Observable<number> {
    return this.http.get<any>('/api/getElementType', {
      responseType: 'json',
      params: new HttpParams()
        .set('elementId', elementId)
    }).pipe(
      map(res => res["response"])
    );
  }
}
