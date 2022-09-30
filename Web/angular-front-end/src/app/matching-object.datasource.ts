import {DataSource} from '@angular/cdk/collections';
import {Observable, ReplaySubject} from 'rxjs';
import { SimpleObject } from "./interfaces/simple-object"

export class MatchingObjectDataSource extends DataSource<SimpleObject> {
  private _dataStream = new ReplaySubject<SimpleObject[]>();

  constructor(initialData: SimpleObject[]) {
    super();
    this.setData(initialData);
  }

  connect(): Observable<SimpleObject[]> {
    return this._dataStream;
  }

  disconnect() {}

  setData(data: SimpleObject[]) {
    this._dataStream.next(data);
  }
}

