import { CollectionViewer, DataSource } from '@angular/cdk/collections';
import { BehaviorSubject, Observable, catchError, of, finalize, tap  } from 'rxjs';
import { WordInfo } from './interfaces/word-info';
import { DataService } from "./services/data.service";
export class WordInfoDataSource implements DataSource<WordInfo> {

  private wordInfosSubject = new BehaviorSubject<WordInfo[]>([]);
  private loadingSubject = new BehaviorSubject<boolean>(false);

  public loading$ = this.loadingSubject.asObservable();

  constructor(private dataService: DataService) {}

  connect(collectionViewer: CollectionViewer): Observable<WordInfo[]> {
    return this.wordInfosSubject.asObservable();
  }

  disconnect(collectionViewer: CollectionViewer): void {
    this.wordInfosSubject.complete();
    this.loadingSubject.complete();
  }

  loadWordInfo(startId: number, endId: number, pageIndex = 0, pageSize = 3) {

    this.loadingSubject.next(true);

    this.dataService.findWordInfo(startId, endId,
      pageIndex, pageSize).pipe(
      catchError(() => of([])),
      finalize(() => this.loadingSubject.next(false)),
      tap( () => console.log('loadWordInfo finished' + startId + '-' + endId))
    )
      .subscribe(wordInfos => this.wordInfosSubject.next(wordInfos));
  }
}
