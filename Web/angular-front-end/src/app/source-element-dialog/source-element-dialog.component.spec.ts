import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SearchElementDialogComponent } from './source-element-dialog.component';

describe('SearchElementDialogComponent', () => {
  let component: SearchElementDialogComponent;
  let fixture: ComponentFixture<SearchElementDialogComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SearchElementDialogComponent ]
    })
    .compileComponents();

    fixture = TestBed.createComponent(SearchElementDialogComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
