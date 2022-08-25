import { CollectionViewer, DataSource } from '@angular/cdk/collections';
import { BehaviorSubject, Observable, catchError, of, finalize, tap  } from 'rxjs';
import { WordInfo } from './interfaces/word-info';
import { DataService } from "./services/data.service";
export class TimelineDataSource implements DataSource<any> {

  private timelineSubject = new BehaviorSubject<any[]>([]);
  private loadingSubject = new BehaviorSubject<boolean>(false);

  public loading$ = this.loadingSubject.asObservable();

  constructor(private dataService: DataService) {}

  connect(collectionViewer: CollectionViewer): Observable<any[]> {
    return this.timelineSubject.asObservable();
  }

  disconnect(collectionViewer: CollectionViewer): void {
    this.timelineSubject.complete();
    this.loadingSubject.complete();
  }

  loadTimelines(pageNumber = 0, pageSize = 3) {

    this.loadingSubject.next(true);

    this.dataService.loadTimelines(pageNumber, pageSize).pipe(
      catchError(() => of([])),
      finalize(() => this.loadingSubject.next(false))
    )
      .subscribe(timeline =>
      {
        this.timelineSubject.next(timeline);
        console.log(timeline);
      });
  }
}
