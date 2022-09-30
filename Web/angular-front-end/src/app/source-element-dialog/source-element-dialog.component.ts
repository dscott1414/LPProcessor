import {Component, Inject, OnInit, Optional, NgZone, Input} from '@angular/core';
import { MatDialogRef, MAT_DIALOG_DATA } from '@angular/material/dialog';
import { MatDialog, MatDialogConfig} from '@angular/material/dialog';
import {FormBuilder, Validators, FormGroup} from "@angular/forms";
import {ViewEncapsulation} from '@angular/core';
import { DataService } from '../services/data.service';
import {FormControl} from '@angular/forms';
import { LpObject } from '../interfaces/lp-object'
import { MatchingObjectDataSource } from "../matching-object.datasource"
import { SimpleObject } from "../interfaces/simple-object"
import { SourceElementTypes } from "../source-element-types"

@Component({
  selector: 'app-source-element-dialog',
  templateUrl: './source-element-dialog.component.html',
  styleUrls: ['./source-element-dialog.component.css']
})
export class SourceElementDialogComponent implements OnInit
{
  public elementId: string = "";
  public matchingType: number;
  public dataService : DataService;
  public matchingObjects: SimpleObject[] = [];
  public surroundingObjects: SimpleObject[] = [];
  public selectedObject: SimpleObject = {} as SimpleObject;
  //public objects: LpObject[] = [];
  private returnData: any = "";
  displayedColumns: string[] = ['name', 'delete'];
  public dataSource: MatchingObjectDataSource = new MatchingObjectDataSource([ ]);

  constructor(public fb: FormBuilder,
    private dialogRef: MatDialogRef<SourceElementDialogComponent>,
    @Optional() @Inject(MAT_DIALOG_DATA) public data: any)
  {
    this.elementId = data['elementId'];
    this.dataService = data['dataService'];
    this.matchingType = data['matchingType'];
    this.dataService.getMatchingObjects(this.elementId, this.matchingType).subscribe(matchingObjects => {
      this.matchingObjects = matchingObjects;
      this.dataSource = new MatchingObjectDataSource(matchingObjects);
    });
    this.dataService.getSurroundingObjects(this.elementId, this.matchingType).subscribe(surroundingObjects => {
      this.surroundingObjects = surroundingObjects;
    });
  }

  setSelectedObject(event:any)
  {
    console.log("selected object",event.value);
    this.selectedObject = event.value;
  }

  deleteMatchingObject(element: any, event: any)
  {
    console.log(element, event);
  }

  objectAlreadyMatched(o: SimpleObject): boolean
  {
    let found = false;
    for (let mo of this.matchingObjects)
      if (mo.id == o.id) {
        found = true;
        break;
      }
    return found;
  }

  removeFromSurroundingObjects(o: SimpleObject): boolean
  {
    let found = false;
    this.surroundingObjects.forEach( (item, index) => {
      if(item.id === o.id) {
        this.surroundingObjects.splice(index, 1);
        found = true;
      }
    });
    return found;
  }

  addMatching()
  {
    this.selectedObject.type = SourceElementTypes.matchingType;
    if (!this.objectAlreadyMatched(this.selectedObject))
      this.matchingObjects.push(this.selectedObject);
    this.removeFromSurroundingObjects(this.selectedObject);
    console.log("matching objects:",this.matchingObjects);
    console.log("surrounding objects:",this.surroundingObjects);
  }

  addAudience()
  {
    this.selectedObject.type = SourceElementTypes.audienceMatchingType;
    if (!this.objectAlreadyMatched(this.selectedObject))
      this.matchingObjects.push(this.selectedObject);
    this.removeFromSurroundingObjects(this.selectedObject);
    console.log("matching objects:",this.matchingObjects);
    console.log("surrounding objects:",this.surroundingObjects);
  }

  close()
  {
      this.dialogRef.close({ event: 'close', data: this.returnData });
  }

  save()
  {
    console.log("saving matching objects:",this.matchingObjects);
    console.log("at:",this.elementId);
    this.dataService.saveMatchingObjects(this.elementId, this.matchingType, this.matchingObjects).subscribe();
    this.dialogRef.close({event: 'save', data: this.returnData});
  }

  ngOnInit(): void {

  }

  onSubmit() {
    console.log("SUBMIT");
  }

}
