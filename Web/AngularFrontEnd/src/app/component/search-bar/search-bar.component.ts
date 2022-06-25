import { Component, OnInit, ViewChild, ElementRef, EventEmitter, Output } from '@angular/core';
import { UntypedFormControl } from '@angular/forms';
import { Observable } from 'rxjs';
import { DataService } from '../data.service';
import { Source } from '../source';

@Component({
  selector: 'app-search-bar',
  templateUrl: './search-bar.component.html',
  styleUrls: ['./search-bar.component.css']
})
export class SearchBarComponent implements OnInit {

  titleControl = new UntypedFormControl();
  authorControl = new UntypedFormControl();
  autoCompleteListAuthors: any[] = [ "" ];
  autoCompleteListTitles: any[] = [ ""];
  authorSearchString: string = "";
  authorSelected: string = "";
  titleSearchString: string = "";
  titleSelected: string = "";

  @ViewChild('autocompleteInputAuthor') autocompleteInputAuthor!: ElementRef;
  @ViewChild('autocompleteInputTitle') autocompleteInputTitle!: ElementRef;
  @Output() onSelectedOption = new EventEmitter();

  constructor(
    public dataService: DataService
  ) { }

  ngOnInit() {
    this.titleControl.valueChanges.subscribe(userInput => {
      this.autoCompleteTitle(userInput);
    })
    this.authorControl.valueChanges.subscribe(userInput => {
      this.autoCompleteAuthor(userInput);
    })
  }

  private autoCompleteTitle(input:string) {
    this.titleSearchString = input;
    this.dataService.getFilteredSourcesList(this.authorSearchString,this.titleSearchString).subscribe(sources => {
      console.log("autoCompleteListTitles=")
      console.log(sources['response']);
      this.autoCompleteListTitles = sources['response']
    });
  }

  private autoCompleteAuthor(input:string) {
    this.authorSearchString = input;
    this.dataService.getFilteredAuthorsList(this.authorSearchString,this.titleSearchString).subscribe(authors => {
      console.log("autoCompleteListAuthors=")
      console.log(authors['response']);
      this.autoCompleteListAuthors = authors['response'];
    });
  }

  // after you clicked an autosuggest option, this function will show the field you want to show in input
  selectValueTitle(selected: Source) {
    console.log(selected);
    let k = selected ? selected.title : selected;
    this.titleSelected = k;
    return k;
  }

  selectValueAuthor(selected: any) {
    console.log(selected);
    let k = selected ? selected.author : selected;
    this.authorSelected = k;
    return k;
  }

  filterTitleEvent(event:any) {
    this.dataService.searchOption.title = event.source.value;
    this.onSelectedOption.emit(this.dataService.searchOption)
    this.focusOnPlaceInputTitle();
  }

  filterAuthorEvent(event:any) {
    this.dataService.searchOption.author = event.source.value;
    this.onSelectedOption.emit(this.dataService.searchOption)
    this.focusOnPlaceInputAuthor();
  }

  // focus the input field and remove any unwanted text.
  focusOnPlaceInputTitle() {
    this.autocompleteInputTitle.nativeElement.focus();
    this.autocompleteInputTitle.nativeElement.value = '';
  }

  focusOnPlaceInputAuthor() {
    this.autocompleteInputAuthor.nativeElement.focus();
    this.autocompleteInputAuthor.nativeElement.value = '';
  }


}
