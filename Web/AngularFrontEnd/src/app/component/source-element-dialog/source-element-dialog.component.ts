import { Component, Inject, OnInit, Optional } from '@angular/core';
import { MatDialogRef, MAT_DIALOG_DATA } from '@angular/material/dialog';
import { MatDialog, MatDialogConfig} from '@angular/material/dialog';
import {FormBuilder, Validators, FormGroup} from "@angular/forms";
//import * as moment from 'moment';
import {ViewEncapsulation} from '@angular/core';
import { DataService } from '../data.service';
import {FormControl} from '@angular/forms';

@Component({
  selector: 'app-source-element-dialog',
  templateUrl: './source-element-dialog.component.html',
  styleUrls: ['./source-element-dialog.component.css']
})
export class SourceElementDialogComponent implements OnInit
{
  //public objectList = new FormControl();
  //public elementId: string = "";
  //public dataService! : DataService;
  //public objects: string[] = ['Tuppence', 'Hi','Hi2'];

  //public form = this.fb.group({
  //  objectList: [''],
  //});

  constructor(//public fb: FormBuilder,
              private dialogRef: MatDialogRef<SourceElementDialogComponent>,
              @Optional() @Inject(MAT_DIALOG_DATA) public data: any)
  {
    //this.elementId = data['elementId'];
    //this.dataService = data['dataService'];
  }

  close()
  {
    console.log("CLOSE");
    this.dialogRef.close({ event: 'close', data: this.returnData });
  }

  save()
  {
    console.log("SAVE");
    this.dialogRef.close({ event: 'save', data: this.returnData });
  }

  private returnData: any;

  ngOnInit(): void {
    this.returnData = "I am from dialog land...";
  }

  onSubmit() {
    console.log("SUBMIT");
    //console.log(this.form.value);
  }
}


